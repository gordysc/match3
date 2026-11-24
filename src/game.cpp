#include "game.h"

#include <raylib.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>

/// @brief Everything that belongs to the Match-3 game.
namespace match3 {

/// @brief Timing constants and helpers internal to `game.cpp`.
namespace {

/// @brief Seconds two gems take to trade places.
constexpr float kSwapSeconds = 0.16f;
/// @brief Seconds an illegal swap takes to bounce back.
constexpr float kBounceSeconds = 0.24f;
/// @brief Seconds cleared gems take to shrink away before gems fall.
constexpr float kClearSeconds = 0.24f;
/// @brief Extra clearing time when a special gem goes off, so its blast
/// can be seen before the gems fall.
constexpr float kBlastExtraSeconds = 0.14f;
/// @brief Shortest fall time, for a gem that drops one cell.
constexpr float kFallBaseSeconds = 0.10f;
/// @brief Extra fall time per square root of a cell fallen. Real falling
/// time grows with the square root of the distance, so long drops speed up.
constexpr float kFallPerCellSeconds = 0.085f;
/// @brief Seconds the "no moves" shuffle animation lasts.
constexpr float kShuffleSeconds = 0.7f;
/// @brief Seconds without a move before a hint appears by itself.
constexpr float kHintDelaySeconds = 7.0f;
/// @brief Seconds a board shake lasts.
constexpr float kShakeSeconds = 0.35f;
/// @brief Fraction of a cell the mouse must drag before a drag swaps.
constexpr float kDragThreshold = 0.4f;

/// @brief Returns true if the mouse pointer actually moved this frame.
///
/// Gating "hide the keyboard cursor" behind real motion stops a resting
/// mouse from fighting keyboard play.
/// @return True when horizontal or vertical mouse delta exceeds half a pixel.
bool MouseMoved() {
  const Vector2 delta = GetMouseDelta();
  return std::abs(delta.x) > 0.5f || std::abs(delta.y) > 0.5f;
}

/// @brief Returns true on the first frame `key` is pressed and on the
/// operating system's key-repeat frames while it is held.
/// @param key raylib key code.
/// @return True if the key should act this frame.
bool KeyPressedOrRepeated(int key) {
  return IsKeyPressed(key) || IsKeyPressedRepeat(key);
}

/// @brief Returns true if two cells are side by side (not diagonal).
/// @param a First cell.
/// @param b Second cell.
/// @return True for up, down, left, or right neighbours.
bool AreNeighbours(GridPoint a, GridPoint b) {
  return std::abs(a.x - b.x) + std::abs(a.y - b.y) == 1;
}

}  // namespace

Game::Game() : save_path_(DefaultSavePath()), save_(LoadSaveData(save_path_)) {
  // Mix wall-clock time into the seed counter so every launch deals fresh
  // boards. `time_since_epoch().count()` is the raw tick count.
  const auto now_ticks = static_cast<std::uint64_t>(
      std::chrono::steady_clock::now().time_since_epoch().count());
  seed_counter_ = static_cast<std::uint32_t>(now_ticks ^ (now_ticks >> 32U));

  mode_ = save_.mode;
  audio_.set_muted(save_.muted);
  StartNewGame();
}

Game::~Game() {
  RecordZenIfPlayed();
  PersistSaveData();
}

void Game::Update(float dt) {
  // Clamp so a long stall (window drag, breakpoint) can't add a huge jump.
  dt = std::clamp(dt, 0.0f, 0.1f);

  HandleKeyboard();
  HandleMouse();
  if (paused_) return;

  now_ += dt;
  effects_.Update(dt, now_);
  shake_timer_ = std::max(0.0f, shake_timer_ - dt);

  // The shown score chases the real one, closing most of the gap each
  // frame, so points visibly count up.
  const float gap = static_cast<float>(score_) - display_score_;
  display_score_ += gap * std::min(1.0f, dt * 10.0f);
  if (std::abs(gap) < 1.0f) display_score_ = static_cast<float>(score_);

  if (mode_ == Mode::Blitz && clock_running_ && phase_ != Phase::Over) {
    time_left_ = std::max(0.0f, time_left_ - dt);
  }

  if (phase_ == Phase::Idle) {
    if (mode_ == Mode::Blitz && clock_running_ && time_left_ <= 0.0f) {
      Settle();  // Time ran out while waiting: end the game.
      return;
    }
    idle_time_ += dt;
    if (idle_time_ >= kHintDelaySeconds && !hint_) hint_ = board_.FindHint();
  }

  // Several phases can end on the same frame (a fall with nothing to drop
  // ends at once). The counter is a safety net against a phase that never
  // sets a future end time.
  for (int guard = 0; guard < 8 && phase_ != Phase::Idle &&
                      phase_ != Phase::Over && now_ >= phase_end_;
       ++guard) {
    AdvancePhase();
  }
}

void Game::Draw() const {
  FrameView view;
  view.mode = mode_;
  view.score = static_cast<int>(std::lround(display_score_));
  view.moves_left = moves_left_;
  view.time_left = time_left_;
  view.clock_running = clock_running_;
  view.record = save_.records[static_cast<std::size_t>(mode_)];
  view.best_cascade = best_cascade_;
  view.new_best = new_best_;
  view.game_over = phase_ == Phase::Over;
  view.over_start = over_start_;
  view.paused = paused_;
  view.muted = audio_.muted();
  view.now = now_;
  view.selected = selected_;
  // `std::optional<T>(value)` wraps a value; the ternary picks either a
  // cursor or "no cursor".
  view.cursor =
      keyboard_active_ ? std::optional<GridPoint>(cursor_) : std::nullopt;
  view.hover = keyboard_active_ ? std::nullopt : hover_;
  view.hint = hint_;
  view.banner_cascade = banner_cascade_;
  view.banner_start = banner_start_;
  view.shuffle_start = shuffle_start_;
  view.combo = combo_;
  view.combo_start = combo_start_;
  view.mouse = GetMousePosition();
  if (shake_timer_ > 0.0f) {
    // Two sine waves at different speeds give a rattle that dies down as
    // the timer runs out.
    const float strength = shake_strength_ * (shake_timer_ / kShakeSeconds);
    view.shake = {strength * std::sin(now_ * 83.0f),
                  strength * std::cos(now_ * 67.0f)};
  }
  renderer_.Draw(board_, layout_, motion_, effects_, view);
}

void Game::StartNewGame() {
  board_.Reset(Board::kDefaultSize, Board::kDefaultSize, NextSeed());
  layout_ = ComputeBoardLayout(board_.width(), board_.height());
  motion_.Reset(board_.width(), board_.height());
  effects_.Clear();

  const ModeRules rules = RulesFor(mode_);
  phase_ = Phase::Idle;
  paused_ = false;
  score_ = 0;
  display_score_ = 0.0f;
  moves_left_ = rules.moves;
  time_left_ = rules.seconds;
  clock_running_ = false;
  best_cascade_ = 0;
  result_recorded_ = false;
  new_best_ = false;
  selected_.reset();
  drag_origin_.reset();
  deselect_on_release_ = false;
  cursor_ = {board_.width() / 2, board_.height() / 2};
  banner_cascade_ = 0;
  shuffle_start_ = -100.0f;
  shake_timer_ = 0.0f;
  combo_ = Combo::None;
  ResetHint();

  // Deal the new board in as a diagonal wave from the top-left corner.
  for (int y = 0; y < board_.height(); ++y) {
    for (int x = 0; x < board_.width(); ++x) {
      motion_.PopIn({x, y}, now_ + 0.025f * static_cast<float>(x + y));
    }
  }
}

void Game::SetMode(Mode mode) {
  // Record a Zen game before `mode_` changes, while it still says Zen.
  RecordZenIfPlayed();
  mode_ = mode;
  save_.mode = mode;
  PersistSaveData();
  StartNewGame();
}

void Game::HandleMouse() {
  const Vector2 mouse = GetMousePosition();
  if (MouseMoved()) keyboard_active_ = false;
  hover_ =
      (paused_ || phase_ == Phase::Over) ? std::nullopt : layout_.CellAt(mouse);

  if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
    for (int i = 0; i < kModeCount; ++i) {
      const auto mode = static_cast<Mode>(i);
      if (CheckCollisionPointRec(mouse, ModeTabRect(mode))) {
        SetMode(mode);
        return;
      }
    }
    if (CheckCollisionPointRec(mouse, NewGameButtonRect())) {
      RecordZenIfPlayed();
      StartNewGame();
      return;
    }
    if (CheckCollisionPointRec(mouse, HintButtonRect())) {
      if (CanMove()) hint_ = board_.FindHint();
      return;
    }
    if (CheckCollisionPointRec(mouse, SoundButtonRect())) {
      audio_.set_muted(!audio_.muted());
      save_.muted = audio_.muted();
      PersistSaveData();
      return;
    }
    if (phase_ == Phase::Over && layout_.CellAt(mouse)) {
      StartNewGame();
      return;
    }
    if (hover_ && CanMove()) {
      const GridPoint cell = *hover_;
      // `*selected_` reads the value inside the optional; it is only used
      // after checking that `selected_` holds one.
      if (selected_ && *selected_ == cell) {
        // Pressed the gem that's already picked up: a drag can still swap
        // it, and a plain click drops it on release.
        deselect_on_release_ = true;
        drag_origin_ = cell;
        drag_start_ = mouse;
      } else if (selected_ && AreNeighbours(*selected_, cell)) {
        TrySwap(*selected_, cell);
      } else if (board_.At(cell)) {
        selected_ = cell;
        audio_.Play(SoundEffect::Select);
        deselect_on_release_ = false;
        drag_origin_ = cell;
        drag_start_ = mouse;
      }
    }
  }

  // Dragging a gem far enough towards a neighbour swaps with it, without
  // waiting for the button to be released.
  if (drag_origin_ && IsMouseButtonDown(MOUSE_BUTTON_LEFT) && CanMove()) {
    const float dx = mouse.x - drag_start_.x;
    const float dy = mouse.y - drag_start_.y;
    const float threshold = layout_.cell_size * kDragThreshold;
    if (std::max(std::abs(dx), std::abs(dy)) > threshold) {
      GridPoint target = *drag_origin_;
      if (std::abs(dx) > std::abs(dy)) {
        target.x += dx > 0.0f ? 1 : -1;
      } else {
        target.y += dy > 0.0f ? 1 : -1;
      }
      const GridPoint origin = *drag_origin_;
      drag_origin_.reset();
      deselect_on_release_ = false;
      TrySwap(origin, target);
    }
  }

  if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
    if (deselect_on_release_) selected_.reset();
    deselect_on_release_ = false;
    drag_origin_.reset();
  }
}

void Game::HandleKeyboard() {
  if (IsKeyPressed(KEY_R) || IsKeyPressed(KEY_F2)) {
    RecordZenIfPlayed();
    StartNewGame();
  }
  if (IsKeyPressed(KEY_ONE)) SetMode(Mode::Moves);
  if (IsKeyPressed(KEY_TWO)) SetMode(Mode::Blitz);
  if (IsKeyPressed(KEY_THREE)) SetMode(Mode::Zen);
  if (IsKeyPressed(KEY_M)) {
    audio_.set_muted(!audio_.muted());
    save_.muted = audio_.muted();
    PersistSaveData();
  }
  if (IsKeyPressed(KEY_P) && phase_ != Phase::Over) paused_ = !paused_;
  if (IsKeyPressed(KEY_H) && CanMove()) hint_ = board_.FindHint();
  if (paused_) return;

  if (phase_ == Phase::Over) {
    if (IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ENTER)) StartNewGame();
    return;
  }

  int dx = 0;
  int dy = 0;
  if (KeyPressedOrRepeated(KEY_LEFT) || KeyPressedOrRepeated(KEY_A)) --dx;
  if (KeyPressedOrRepeated(KEY_RIGHT) || KeyPressedOrRepeated(KEY_D)) ++dx;
  if (KeyPressedOrRepeated(KEY_UP) || KeyPressedOrRepeated(KEY_W)) --dy;
  if (KeyPressedOrRepeated(KEY_DOWN) || KeyPressedOrRepeated(KEY_S)) ++dy;
  // Ignore diagonals (two arrows on the same frame): gems only swap
  // sideways or up and down.
  if ((dx != 0) != (dy != 0)) {
    if (!keyboard_active_) {
      // The first key press only shows the cursor where it was.
      keyboard_active_ = true;
    } else if (selected_ && CanMove()) {
      // With a gem picked up, an arrow swaps it that way, and the cursor
      // follows the gem.
      const GridPoint target{selected_->x + dx, selected_->y + dy};
      if (board_.InBounds(target)) {
        TrySwap(*selected_, target);
        cursor_ = target;
      }
    } else {
      cursor_.x = std::clamp(cursor_.x + dx, 0, board_.width() - 1);
      cursor_.y = std::clamp(cursor_.y + dy, 0, board_.height() - 1);
    }
  }

  if (IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ENTER)) {
    keyboard_active_ = true;
    SelectOrSwap(cursor_);
  }
}

bool Game::CanMove() const {
  if (phase_ != Phase::Idle || paused_) return false;
  if (mode_ == Mode::Moves && moves_left_ <= 0) return false;
  if (mode_ == Mode::Blitz && clock_running_ && time_left_ <= 0.0f) {
    return false;
  }
  return true;
}

void Game::SelectOrSwap(GridPoint cell) {
  if (!CanMove()) return;
  if (selected_) {
    if (*selected_ == cell) {
      selected_.reset();
      return;
    }
    if (AreNeighbours(*selected_, cell)) {
      TrySwap(*selected_, cell);
      return;
    }
  }
  if (board_.At(cell)) {
    selected_ = cell;
    audio_.Play(SoundEffect::Select);
  }
}

void Game::TrySwap(GridPoint a, GridPoint b) {
  if (!CanMove() || !board_.InBounds(a) || !board_.InBounds(b)) return;
  selected_.reset();
  ResetHint();
  if (board_.ApplySwap(a, b)) {
    // The Board has already swapped the gems; each one appears to start
    // from the other's cell and slide home.
    motion_.Slide(a, {static_cast<float>(b.x), static_cast<float>(b.y)},
                  MotionKind::Swap, now_, kSwapSeconds);
    motion_.Slide(b, {static_cast<float>(a.x), static_cast<float>(a.y)},
                  MotionKind::Swap, now_, kSwapSeconds);
    if (mode_ == Mode::Moves) --moves_left_;
    clock_running_ = true;
    audio_.Play(SoundEffect::Swap);
    phase_ = Phase::Swapping;
    phase_end_ = now_ + kSwapSeconds;
  } else if (AreNeighbours(a, b) && board_.At(a) && board_.At(b)) {
    motion_.Bump(a, b, now_, kBounceSeconds);
    motion_.Bump(b, a, now_, kBounceSeconds);
    audio_.Play(SoundEffect::Invalid);
    phase_ = Phase::Bouncing;
    phase_end_ = now_ + kBounceSeconds;
  }
}

void Game::AdvancePhase() {
  switch (phase_) {
    case Phase::Swapping:
    case Phase::Falling:
      RunClearStep();
      break;
    case Phase::Clearing:
      RunCollapse();
      break;
    case Phase::Bouncing:
    case Phase::Shuffling:
      phase_ = Phase::Idle;
      break;
    case Phase::Idle:
    case Phase::Over:
      break;
  }
}

void Game::RunClearStep() {
  // `auto` here deduces `std::optional<ClearStep>`.
  const auto step = board_.ClearMatches();
  if (!step) {
    Settle();
    return;
  }
  score_ += step->points;
  best_cascade_ = std::max(best_cascade_, step->cascade);

  // Ghosts and sparks for every cleared gem, and a score number at their
  // average position.
  float sum_x = 0.0f;
  float sum_y = 0.0f;
  for (const PlacedGem& cleared : step->cleared) {
    effects_.AddGhost(cleared, now_);
    const Vector2 centre = layout_.CellCentre(static_cast<float>(cleared.at.x),
                                              static_cast<float>(cleared.at.y));
    effects_.AddBurst(centre.x, centre.y, cleared.gem.color, 7, 260.0f);
    sum_x += static_cast<float>(cleared.at.x);
    sum_y += static_cast<float>(cleared.at.y);
  }
  const auto count = static_cast<float>(step->cleared.size());
  effects_.AddText(
      {{sum_x / count, sum_y / count}, step->points, step->cascade, now_});

  bool line = false;
  bool bomb = false;
  bool rainbow = false;
  for (const Blast& blast : step->blasts) {
    switch (blast.special) {
      case Special::None:
        break;
      case Special::LineH:
      case Special::LineV: {
        const bool horizontal = blast.special == Special::LineH;
        effects_.AddBeam(
            {horizontal ? blast.at.y : blast.at.x, horizontal, blast.at, now_});
        line = true;
        break;
      }
      case Special::Bomb:
        // The ring reaches just past the edge of the square it clears:
        // radius 1.6 cells for the 3x3 Bomb, 2.6 for the 5x5 Mega Bomb.
        effects_.AddShockwave(
            {blast.at, 0.6f + static_cast<float>(blast.reach), false, now_});
        bomb = true;
        break;
      case Special::Rainbow:
        effects_.AddShockwave({blast.at, 4.0f, true, now_});
        rainbow = true;
        break;
    }
  }
  // New specials grow in once the gems they were made from have shrunk.
  for (const PlacedGem& created : step->created) {
    motion_.PopIn(created.at, now_ + Effects::kGhostSeconds * 0.5f);
  }

  audio_.PlayCascade(step->cascade);
  if (!step->created.empty()) audio_.Play(SoundEffect::Create);
  if (line) audio_.Play(SoundEffect::Line);
  if (bomb) audio_.Play(SoundEffect::Bomb);
  if (rainbow) audio_.Play(SoundEffect::Rainbow);

  const bool combo = step->combo != Combo::None;
  if (combo) {
    combo_ = step->combo;
    combo_start_ = now_;
    // Layer an extra sound on top so a combo sounds bigger than either
    // special alone.
    audio_.Play(SoundEffect::Rainbow);
  }

  float strength = rainbow ? 11.0f : (bomb ? 8.0f : (line ? 4.0f : 0.0f));
  if (combo) strength = 14.0f;
  if (strength > 0.0f) {
    // A weak blast never cuts a strong shake that is still running short.
    if (shake_timer_ <= 0.0f || strength >= shake_strength_) {
      shake_strength_ = strength;
    }
    shake_timer_ = kShakeSeconds;
  }
  if (step->cascade >= 2) {
    banner_cascade_ = step->cascade;
    banner_start_ = now_;
  }

  phase_ = Phase::Clearing;
  phase_end_ =
      now_ + kClearSeconds + (step->blasts.empty() ? 0.0f : kBlastExtraSeconds);
}

void Game::RunCollapse() {
  float longest = 0.0f;
  for (const FallMove& move : board_.Collapse()) {
    const float distance = static_cast<float>(move.to.y) - move.from_y;
    const float duration =
        kFallBaseSeconds + kFallPerCellSeconds * std::sqrt(distance);
    motion_.Slide(move.to, {static_cast<float>(move.to.x), move.from_y},
                  MotionKind::Fall, now_, duration);
    longest = std::max(longest, duration);
  }
  phase_ = Phase::Falling;
  phase_end_ = now_ + longest;
}

void Game::Settle() {
  phase_ = Phase::Idle;
  const bool out_of_moves = mode_ == Mode::Moves && moves_left_ <= 0;
  const bool out_of_time =
      mode_ == Mode::Blitz && clock_running_ && time_left_ <= 0.0f;
  if (out_of_moves || out_of_time) {
    EndGame();
    return;
  }
  if (!board_.HasLegalMove()) {
    board_.Shuffle();
    for (int y = 0; y < board_.height(); ++y) {
      for (int x = 0; x < board_.width(); ++x) {
        motion_.PopIn({x, y}, now_ + 0.02f * static_cast<float>(x + y));
      }
    }
    shuffle_start_ = now_;
    audio_.Play(SoundEffect::Shuffle);
    phase_ = Phase::Shuffling;
    phase_end_ = now_ + kShuffleSeconds;
  }
  ResetHint();
}

void Game::EndGame() {
  phase_ = Phase::Over;
  over_start_ = now_;
  selected_.reset();
  hint_.reset();
  if (!result_recorded_) {
    result_recorded_ = true;
    new_best_ = RecordResult(save_, mode_, score_, best_cascade_);
    PersistSaveData();
  }
  audio_.Play(SoundEffect::GameOver);
}

void Game::RecordZenIfPlayed() {
  if (mode_ != Mode::Zen || result_recorded_ || score_ <= 0) return;
  result_recorded_ = true;
  RecordResult(save_, mode_, score_, best_cascade_);
  PersistSaveData();
}

void Game::ResetHint() {
  hint_.reset();
  idle_time_ = 0.0f;
}

void Game::PersistSaveData() { WriteSaveData(save_path_, save_); }

std::uint32_t Game::NextSeed() {
  // Multiply by a large odd constant (Knuth's multiplicative hash) so
  // consecutive counters give very different seeds. Unsigned overflow wraps
  // around by definition, so this is well-defined.
  seed_counter_ += 1U;
  return seed_counter_ * 2654435761U;
}

}  // namespace match3
