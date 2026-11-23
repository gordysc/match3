#ifndef MATCH3_EFFECTS_H_
#define MATCH3_EFFECTS_H_

#include <cstdint>
#include <random>
#include <span>
#include <vector>

#include "board.h"
#include "gem.h"

/// @brief Everything that belongs to the Match-3 game.
namespace match3 {

/// @brief A position measured in cells, not pixels. `(2.5, 3.0)` is halfway
/// between columns 2 and 3 on row 3.
struct CellPosition {
  /// @brief Column, possibly fractional.
  float x = 0.0f;
  /// @brief Row, possibly fractional (negative = above the board).
  float y = 0.0f;
};

/// @brief How a gem moves on its way to its cell.
enum class MotionKind : std::uint8_t {
  /// @brief Not moving.
  None,
  /// @brief Trading places with a neighbour: eases in and out.
  Swap,
  /// @brief Falling: speeds up like gravity, then lands with a small bounce.
  Fall,
  /// @brief An illegal swap: moves partway towards a neighbour and back.
  Bump,
};

/// @brief Remembers how each gem on the board is moving, so the renderer
/// can draw it between cells.
///
/// The Board always holds every gem at its final cell. The tracker only
/// stores where each gem *appears* to come from and when it started, so the
/// animation never changes the rules.
class MotionTracker {
 public:
  /// @brief Forgets all motion and sizes storage for a board.
  /// @param columns Board width.
  /// @param rows Board height.
  void Reset(int columns, int rows);

  /// @brief Makes the gem in `cell` slide in from `from`.
  /// @param cell Cell the gem now sits in.
  /// @param from Where it appears to start, in cells.
  /// @param kind `MotionKind::Swap` or `MotionKind::Fall`.
  /// @param now Animation clock in seconds.
  /// @param duration Length of the slide in seconds.
  void Slide(GridPoint cell, CellPosition from, MotionKind kind, float now,
             float duration);

  /// @brief Makes the gem in `cell` lean towards `toward` and come back.
  /// @param cell Cell the gem sits in (and stays in).
  /// @param toward Neighbouring cell it leans towards.
  /// @param now Animation clock in seconds.
  /// @param duration Length of the bump in seconds.
  void Bump(GridPoint cell, GridPoint toward, float now, float duration);

  /// @brief Makes the gem in `cell` grow in from nothing.
  /// @param cell Cell to animate.
  /// @param start Animation clock time when it starts growing.
  void PopIn(GridPoint cell, float start);

  /// @brief Returns where the gem in `(x, y)` should be drawn.
  /// @param x Column index.
  /// @param y Row index.
  /// @param now Animation clock in seconds.
  /// @return Position in cells.
  CellPosition Position(int x, int y, float now) const;

  /// @brief Returns how large the gem in `(x, y)` should be drawn.
  /// @param x Column index.
  /// @param y Row index.
  /// @param now Animation clock in seconds.
  /// @return `1.0` for full size; pop-ins grow from `0.0` and overshoot a
  ///         little. Falls squash slightly as they land.
  float Scale(int x, int y, float now) const;

 private:
  /// @brief One gem's motion.
  struct Motion {
    /// @brief Kind of movement.
    MotionKind kind = MotionKind::None;
    /// @brief Start offset from the cell (for slides) or bump direction.
    CellPosition offset;
    /// @brief Animation clock time when the motion began.
    float start = 0.0f;
    /// @brief Length of the motion in seconds.
    float duration = 0.0f;
    /// @brief Time the pop-in began (far in the past = no pop-in).
    float pop_start = -1000.0f;
  };

  /// @brief Returns the motion slot for `(x, y)`, or `nullptr` out of bounds.
  /// @param x Column index.
  /// @param y Row index.
  /// @return Non-owning pointer into `motions_`. It stays valid until the
  ///         next `Reset()`, which may reallocate the vector.
  const Motion* Find(int x, int y) const;

  /// @brief Non-const overload of `Find()` for the methods that write.
  /// @param x Column index.
  /// @param y Row index.
  /// @return Non-owning pointer into `motions_`, or `nullptr`.
  Motion* Find(int x, int y);

  /// @brief Board width, for turning `(x, y)` into an index.
  int columns_ = 0;
  /// @brief Board height.
  int rows_ = 0;
  /// @brief One Motion per cell, row-major.
  std::vector<Motion> motions_;
};

/// @brief A gem that was just cleared, shrinking away where it was.
struct Ghost {
  /// @brief The gem and its cell.
  PlacedGem gem;
  /// @brief Animation clock time when it was cleared.
  float start = 0.0f;
};

/// @brief One spark flying away from a cleared gem.
struct Particle {
  /// @brief Horizontal position in pixels.
  float x = 0.0f;
  /// @brief Vertical position in pixels.
  float y = 0.0f;
  /// @brief Horizontal speed in pixels per second.
  float vx = 0.0f;
  /// @brief Vertical speed in pixels per second.
  float vy = 0.0f;
  /// @brief Seconds left before it disappears.
  float life = 0.0f;
  /// @brief Seconds it lived for in total.
  float max_life = 1.0f;
  /// @brief Radius in pixels at full life.
  float size = 3.0f;
  /// @brief Colour of the gem it came from.
  GemColor color = GemColor::Red;
  /// @brief True for a white spark instead of a coloured one.
  bool white = false;
};

/// @brief A "+120" score number floating up from a clear.
struct FloatingText {
  /// @brief Where it starts, in cells.
  CellPosition at;
  /// @brief Points shown.
  int points = 0;
  /// @brief Cascade step it came from (higher = bigger text).
  int cascade = 1;
  /// @brief Animation clock time when it appeared.
  float start = 0.0f;
};

/// @brief A flash along a row or column when a Line gem goes off.
struct Beam {
  /// @brief Row (horizontal beam) or column (vertical beam).
  int index = 0;
  /// @brief True for a row, false for a column.
  bool horizontal = true;
  /// @brief Cell the Line gem was in, where the beam starts.
  GridPoint origin;
  /// @brief Animation clock time when it fired.
  float start = 0.0f;
};

/// @brief An expanding shock ring when a Bomb or Rainbow goes off.
struct Shockwave {
  /// @brief Centre, in cells.
  GridPoint at;
  /// @brief Final radius, in cells.
  float radius = 1.5f;
  /// @brief True for the multicoloured Rainbow ring.
  bool rainbow = false;
  /// @brief Animation clock time when it fired.
  float start = 0.0f;
};

/// @brief Short-lived eye candy: ghosts, sparks, score numbers, beams, and
/// shockwaves. Nothing here affects the rules; it only remembers what to
/// draw and forgets each item once its animation is over.
class Effects {
 public:
  /// @brief Seconds a ghost takes to shrink away.
  static constexpr float kGhostSeconds = 0.28f;
  /// @brief Seconds a score number floats.
  static constexpr float kTextSeconds = 0.9f;
  /// @brief Seconds a beam flashes.
  static constexpr float kBeamSeconds = 0.35f;
  /// @brief Seconds a shockwave expands.
  static constexpr float kShockSeconds = 0.45f;

  /// @brief Removes every effect.
  void Clear();

  /// @brief Moves particles and drops effects that have finished.
  /// @param dt Seconds since the last frame.
  /// @param now Animation clock in seconds.
  void Update(float dt, float now);

  /// @brief Adds a shrinking ghost.
  /// @param gem The cleared gem and its cell.
  /// @param now Animation clock in seconds.
  void AddGhost(PlacedGem gem, float now);

  /// @brief Throws `count` sparks out from a point.
  /// @param x Horizontal position in pixels.
  /// @param y Vertical position in pixels.
  /// @param color Colour of the gem they came from.
  /// @param count Number of sparks.
  /// @param speed Typical speed in pixels per second.
  void AddBurst(float x, float y, GemColor color, int count, float speed);

  /// @brief Adds a floating score number.
  /// @param text The number and where it starts.
  void AddText(FloatingText text);

  /// @brief Adds a Line gem beam.
  /// @param beam The row or column and when it fired.
  void AddBeam(Beam beam);

  /// @brief Adds a Bomb or Rainbow shockwave.
  /// @param wave The centre, size, and when it fired.
  void AddShockwave(Shockwave wave);

  /// @brief Ghosts still shrinking.
  /// @return Read-only view; valid until the next non-const call.
  std::span<const Ghost> ghosts() const { return ghosts_; }
  /// @brief Sparks still flying.
  /// @return Read-only view; valid until the next non-const call.
  std::span<const Particle> particles() const { return particles_; }
  /// @brief Score numbers still floating.
  /// @return Read-only view; valid until the next non-const call.
  std::span<const FloatingText> texts() const { return texts_; }
  /// @brief Beams still flashing.
  /// @return Read-only view; valid until the next non-const call.
  std::span<const Beam> beams() const { return beams_; }
  /// @brief Shockwaves still expanding.
  /// @return Read-only view; valid until the next non-const call.
  std::span<const Shockwave> shockwaves() const { return shockwaves_; }

 private:
  /// @brief Ghosts still shrinking.
  std::vector<Ghost> ghosts_;
  /// @brief Sparks still flying.
  std::vector<Particle> particles_;
  /// @brief Score numbers still floating.
  std::vector<FloatingText> texts_;
  /// @brief Beams still flashing.
  std::vector<Beam> beams_;
  /// @brief Shockwaves still expanding.
  std::vector<Shockwave> shockwaves_;
  /// @brief Random source for spark directions (looks only, so a fixed
  /// seed is fine).
  std::mt19937 rng_{12345U};
};

}  // namespace match3

#endif  // MATCH3_EFFECTS_H_
