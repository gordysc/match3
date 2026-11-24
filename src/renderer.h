#ifndef MATCH3_RENDERER_H_
#define MATCH3_RENDERER_H_

#include <raylib.h>

#include <optional>

#include "board.h"
#include "effects.h"
#include "layout.h"
#include "mode.h"
#include "storage.h"

/// @brief Everything that belongs to the Match-3 game.
namespace match3 {

/// @brief Read-only snapshot of UI state handed from Game to Renderer, so
/// drawing stays completely `const` and side-effect-free.
struct FrameView {
  /// @brief Mode being played.
  Mode mode = Mode::Moves;
  /// @brief Score to show (counts up towards the real score).
  int score = 0;
  /// @brief Swaps left in Moves mode.
  int moves_left = 0;
  /// @brief Seconds left in Blitz mode.
  float time_left = 0.0f;
  /// @brief True once the Blitz clock has started (on the first swap).
  bool clock_running = false;
  /// @brief Saved results for `mode`.
  ModeRecord record{};
  /// @brief Longest cascade chain in this game so far.
  int best_cascade = 0;
  /// @brief True when the finished game beat the saved best score.
  bool new_best = false;
  /// @brief True once the game has ended.
  bool game_over = false;
  /// @brief Animation clock time when the game ended (for the fade-in).
  float over_start = 0.0f;
  /// @brief True while paused (the board is hidden).
  bool paused = false;
  /// @brief True if sound is off.
  bool muted = false;
  /// @brief Animation clock in seconds (stops while paused).
  float now = 0.0f;
  /// @brief Gem the player picked up, if any.
  std::optional<GridPoint> selected;
  /// @brief Keyboard cursor, shown only while the keyboard is in use.
  std::optional<GridPoint> cursor;
  /// @brief Cell under the mouse pointer, if any.
  std::optional<GridPoint> hover;
  /// @brief Legal swap to point out, if a hint is showing.
  std::optional<Swap> hint;
  /// @brief Cascade step to celebrate in a banner (`0` = none).
  int banner_cascade = 0;
  /// @brief Animation clock time when the banner appeared.
  float banner_start = 0.0f;
  /// @brief Last combo swap, to name in a banner (`Combo::None` = none).
  Combo combo = Combo::None;
  /// @brief Animation clock time when the combo went off.
  float combo_start = -100.0f;
  /// @brief Animation clock time of the last "no moves" shuffle.
  float shuffle_start = -100.0f;
  /// @brief Board offset used to shake the board after big blasts.
  Vector2 shake{0.0f, 0.0f};
  /// @brief Mouse position, for hover highlights on buttons.
  Vector2 mouse{0.0f, 0.0f};
};

/// @brief Stateless drawing subsystem: turns the board, its animations, and
/// a FrameView into one frame of raylib draw calls.
class Renderer {
 public:
  /// @brief Draws the whole window. Call between `BeginDrawing()` and
  /// `EndDrawing()`.
  /// @param board Board to draw.
  /// @param layout Where the board sits on screen.
  /// @param motion Where each gem is in its slide or fall.
  /// @param effects Ghosts, sparks, beams, and score numbers.
  /// @param view UI state for this frame.
  void Draw(const Board& board, const BoardLayout& layout,
            const MotionTracker& motion, const Effects& effects,
            const FrameView& view) const;
};

}  // namespace match3

#endif  // MATCH3_RENDERER_H_
