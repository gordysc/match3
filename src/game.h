#ifndef MATCH3_GAME_H_
#define MATCH3_GAME_H_

#include <raylib.h>

#include <cstdint>
#include <filesystem>
#include <optional>

#include "audio.h"
#include "board.h"
#include "effects.h"
#include "layout.h"
#include "mode.h"
#include "renderer.h"
#include "storage.h"

/// @brief Everything that belongs to the Match-3 game.
namespace match3 {

/// @brief What the game is doing right now. Every phase except `Idle` and
/// `Over` is an animation that ends at a set time, then moves on.
///
/// A move runs: Swapping -> Clearing -> Falling -> Clearing -> Falling ...
/// until a clear finds nothing, then back to Idle (or Over). An illegal swap
/// runs Bouncing -> Idle.
enum class Phase : std::uint8_t {
  /// @brief Waiting for the player.
  Idle,
  /// @brief Two gems are trading places.
  Swapping,
  /// @brief Two gems lean towards each other and back (illegal swap).
  Bouncing,
  /// @brief Cleared gems are shrinking away.
  Clearing,
  /// @brief Gems are falling into the holes.
  Falling,
  /// @brief A dead board is being reshuffled.
  Shuffling,
  /// @brief The game has ended; waiting for a new game.
  Over,
};

/// @brief Top-level coordinator: reads input, drives the Board one step at
/// a time, runs the animations and sounds, keeps score, records results,
/// and hands everything to the Renderer.
///
/// `main()` constructs one `Game` inside an explicit RAII scope block so its
/// destructor (which saves progress and unloads sounds) runs before
/// `CloseAudioDevice()` and `CloseWindow()`.
class Game {
 public:
  /// @brief Loads saved records and starts a game in the last mode.
  Game();

  /// @brief Saves settings and records on clean shutdown.
  ~Game();

  /// @brief Non-copyable: there is only ever one game, and a copy would save
  /// the same file twice when both copies are destroyed.
  Game(const Game&) = delete;
  /// @brief Non-copy-assignable for the same reason.
  Game& operator=(const Game&) = delete;

  /// @brief Handles input and advances animations by one frame.
  /// @param dt Seconds elapsed since the previous frame.
  void Update(float dt);

  /// @brief Renders the current frame between raylib's `BeginDrawing()` and
  /// `EndDrawing()`. Marked `const` so drawing can never mutate game state.
  void Draw() const;

 private:
  /// @brief Deals a fresh board in the current mode.
  void StartNewGame();

  /// @brief Switches mode (saving the choice) and starts a new game.
  /// @param mode Mode to switch to.
  void SetMode(Mode mode);

  /// @brief Handles clicks and drags on the board, tabs, and buttons.
  void HandleMouse();

  /// @brief Handles hotkeys and keyboard-only play.
  void HandleKeyboard();

  /// @brief Returns true if the player may pick up and swap gems now.
  /// @return True while idle, not paused, and not out of moves or time.
  bool CanMove() const;

  /// @brief Picks up `cell`, or swaps with it if a neighbour is picked up.
  /// @param cell Cell the player clicked or pressed Space on.
  void SelectOrSwap(GridPoint cell);

  /// @brief Tries to swap two neighbouring gems and starts the matching
  /// animation (a real swap or a bounce back).
  /// @param a Cell the gem is picked up from.
  /// @param b Neighbouring cell it moves to.
  void TrySwap(GridPoint a, GridPoint b);

  /// @brief Moves on from the current phase when its animation is over.
  void AdvancePhase();

  /// @brief Runs one `Board::ClearMatches()` and starts its effects, or
  /// finishes the move if nothing matched.
  void RunClearStep();

  /// @brief Runs `Board::Collapse()` and starts the falling animation.
  void RunCollapse();

  /// @brief Called when a move is completely finished: ends the game if the
  /// moves or time ran out, reshuffles a dead board, or goes back to Idle.
  void Settle();

  /// @brief Ends the game and records its result.
  void EndGame();

  /// @brief Records a Zen game that is being left (restart, mode change,
  /// or quit). Zen never ends by itself, so this is where its score counts.
  void RecordZenIfPlayed();

  /// @brief Clears the hint and restarts the idle timer that shows it.
  void ResetHint();

  /// @brief Writes `save_` to `save_path_` so records persist.
  void PersistSaveData();

  /// @brief Generates a fresh seed for a new board.
  /// @return 32-bit PRNG seed.
  std::uint32_t NextSeed();

  /// @brief Filesystem path where records are stored.
  std::filesystem::path save_path_;
  /// @brief Loaded settings and records.
  SaveData save_{};
  /// @brief Mode currently being played.
  Mode mode_ = Mode::Moves;
  /// @brief The rules and state of the current game.
  Board board_;
  /// @brief Screen placement of `board_`'s cells.
  BoardLayout layout_{};
  /// @brief Gem slides, falls, and pop-ins.
  MotionTracker motion_;
  /// @brief Ghosts, sparks, beams, and score numbers.
  Effects effects_;
  /// @brief Synthesised sounds.
  Audio audio_;
  /// @brief Stateless drawing subsystem.
  Renderer renderer_;

  /// @brief What the game is doing now.
  Phase phase_ = Phase::Idle;
  /// @brief Animation clock time when the current phase ends.
  float phase_end_ = 0.0f;
  /// @brief Animation clock in seconds; stops while paused.
  float now_ = 0.0f;
  /// @brief True while paused.
  bool paused_ = false;

  /// @brief Real score.
  int score_ = 0;
  /// @brief Score shown on screen; counts up towards `score_`.
  float display_score_ = 0.0f;
  /// @brief Swaps left (Moves mode).
  int moves_left_ = 0;
  /// @brief Seconds left (Blitz mode).
  float time_left_ = 0.0f;
  /// @brief True once the first swap has started the Blitz clock.
  bool clock_running_ = false;
  /// @brief Longest cascade in this game.
  int best_cascade_ = 0;
  /// @brief True once this game's result has been recorded.
  bool result_recorded_ = false;
  /// @brief True when this game set a new best score.
  bool new_best_ = false;
  /// @brief Animation clock time when the game ended.
  float over_start_ = 0.0f;

  /// @brief Gem the player picked up, if any.
  std::optional<GridPoint> selected_;
  /// @brief Cell under the mouse, if any.
  std::optional<GridPoint> hover_;
  /// @brief Cell a mouse drag started on, while the button is held.
  std::optional<GridPoint> drag_origin_;
  /// @brief Mouse position where the drag started.
  Vector2 drag_start_{0.0f, 0.0f};
  /// @brief True if releasing the mouse should drop the picked-up gem
  /// (the press landed on the gem that was already picked up).
  bool deselect_on_release_ = false;
  /// @brief Keyboard cursor position.
  GridPoint cursor_{};
  /// @brief True while the keyboard is in use; moving the mouse hides the
  /// cursor again.
  bool keyboard_active_ = false;

  /// @brief Seconds spent idle since the last move, for the automatic hint.
  float idle_time_ = 0.0f;
  /// @brief The hint being shown, if any.
  std::optional<Swap> hint_;

  /// @brief Seconds of board shake left.
  float shake_timer_ = 0.0f;
  /// @brief Strength of the current shake in pixels.
  float shake_strength_ = 0.0f;
  /// @brief Cascade step shown in the banner.
  int banner_cascade_ = 0;
  /// @brief Animation clock time when the banner appeared.
  float banner_start_ = -100.0f;
  /// @brief Animation clock time of the last "no moves" shuffle.
  float shuffle_start_ = -100.0f;
  /// @brief Last combo the player set off, for its banner.
  Combo combo_ = Combo::None;
  /// @brief Animation clock time when `combo_` went off.
  float combo_start_ = -100.0f;

  /// @brief Monotonic counter mixed into seeds so each board is different.
  std::uint32_t seed_counter_ = 1;
};

}  // namespace match3

#endif  // MATCH3_GAME_H_
