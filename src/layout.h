#ifndef MATCH3_LAYOUT_H_
#define MATCH3_LAYOUT_H_

#include <raylib.h>

#include <optional>

#include "gem.h"
#include "mode.h"

/// @brief Everything that belongs to the Match-3 game.
namespace match3 {

/// @brief Where the board's cells sit on screen.
///
/// `Game` uses it to turn mouse positions into cells, and `Renderer` uses
/// the same values to draw, so input and graphics can never disagree.
struct BoardLayout {
  /// @brief Left edge of column 0 in pixels.
  float origin_x = 0.0f;
  /// @brief Top edge of row 0 in pixels.
  float origin_y = 0.0f;
  /// @brief Width and height of one cell in pixels.
  float cell_size = 80.0f;
  /// @brief Number of columns on the board.
  int columns = 0;
  /// @brief Number of rows on the board.
  int rows = 0;

  /// @brief Returns the rectangle covering every cell.
  /// @return Board bounds in pixels.
  Rectangle Bounds() const;

  /// @brief Returns the screen rectangle of cell `(x, y)`.
  /// @param x Column index.
  /// @param y Row index.
  /// @return Cell bounds in pixels.
  Rectangle CellRect(int x, int y) const;

  /// @brief Returns the screen point at the centre of a cell position.
  ///
  /// Takes floats so animations can ask for in-between spots, such as a gem
  /// halfway through falling from row 2 to row 3 (`y = 2.5`).
  /// @param x Column, possibly fractional.
  /// @param y Row, possibly fractional (negative = above the board).
  /// @return Centre in pixels.
  Vector2 CellCentre(float x, float y) const;

  /// @brief Finds the cell under `point`.
  /// @param point Screen position, usually the mouse.
  /// @return The cell, or `std::nullopt` when `point` is off the board.
  std::optional<GridPoint> CellAt(Vector2 point) const;
};

/// @brief Fits a `columns` x `rows` board into the area left of the sidebar,
/// as large as fits and centred.
/// @param columns Number of columns.
/// @param rows Number of rows.
/// @return Layout for that board.
BoardLayout ComputeBoardLayout(int columns, int rows);

/// @brief The sidebar on the right holding scores and buttons.
/// @return Sidebar bounds in pixels.
Rectangle SidebarRect();

/// @brief Clickable tab that switches to `mode`.
/// @param mode Tab to locate.
/// @return Tab bounds in pixels.
Rectangle ModeTabRect(Mode mode);

/// @brief Panel showing the current score.
/// @return Panel bounds in pixels.
Rectangle ScorePanelRect();

/// @brief Panel showing moves left, time left, or the cascade record.
/// @return Panel bounds in pixels.
Rectangle LimitPanelRect();

/// @brief Panel showing the saved best score for the mode.
/// @return Panel bounds in pixels.
Rectangle BestPanelRect();

/// @brief "New game" button.
/// @return Button bounds in pixels.
Rectangle NewGameButtonRect();

/// @brief Sound on/off button.
/// @return Button bounds in pixels.
Rectangle SoundButtonRect();

/// @brief Hint button.
/// @return Button bounds in pixels.
Rectangle HintButtonRect();

}  // namespace match3

#endif  // MATCH3_LAYOUT_H_
