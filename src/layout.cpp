#include "layout.h"

#include <algorithm>
#include <cmath>

#include "screen.h"

/// @brief Everything that belongs to the Match-3 game.
namespace match3 {

/// @brief Layout constants internal to `layout.cpp`.
namespace {

/// @brief Space between the window edge and the board or sidebar.
constexpr float kMargin = 40.0f;
/// @brief Width of the sidebar.
constexpr float kSidebarWidth = 300.0f;
/// @brief Largest cell size, so small boards don't get huge gems.
constexpr float kMaxCellSize = 84.0f;
/// @brief Space between sidebar widgets.
constexpr float kGap = 14.0f;
/// @brief Inner padding of the sidebar.
constexpr float kPadding = 20.0f;

/// @brief Width of the widgets inside the sidebar.
/// @return Pixels.
float InnerWidth() { return kSidebarWidth - 2.0f * kPadding; }

/// @brief Left edge of the widgets inside the sidebar.
/// @return Pixels.
float InnerLeft() { return SidebarRect().x + kPadding; }

}  // namespace

Rectangle BoardLayout::Bounds() const {
  return {origin_x, origin_y, cell_size * static_cast<float>(columns),
          cell_size * static_cast<float>(rows)};
}

Rectangle BoardLayout::CellRect(int x, int y) const {
  return {origin_x + cell_size * static_cast<float>(x),
          origin_y + cell_size * static_cast<float>(y), cell_size, cell_size};
}

Vector2 BoardLayout::CellCentre(float x, float y) const {
  return {origin_x + cell_size * (x + 0.5f), origin_y + cell_size * (y + 0.5f)};
}

std::optional<GridPoint> BoardLayout::CellAt(Vector2 point) const {
  // `std::floor` (not a plain cast) so points just left of or above the
  // board give -1 instead of being rounded towards zero into column 0.
  const int x = static_cast<int>(std::floor((point.x - origin_x) / cell_size));
  const int y = static_cast<int>(std::floor((point.y - origin_y) / cell_size));
  if (x < 0 || x >= columns || y < 0 || y >= rows) return std::nullopt;
  return GridPoint{x, y};
}

BoardLayout ComputeBoardLayout(int columns, int rows) {
  const float area_width =
      static_cast<float>(kScreenWidth) - kSidebarWidth - 3.0f * kMargin;
  const float area_height = static_cast<float>(kScreenHeight) - 2.0f * kMargin;
  BoardLayout layout;
  layout.columns = columns;
  layout.rows = rows;
  layout.cell_size = std::floor(
      std::min({kMaxCellSize, area_width / static_cast<float>(columns),
                area_height / static_cast<float>(rows)}));
  const Rectangle bounds = layout.Bounds();
  layout.origin_x = kMargin + (area_width - bounds.width) / 2.0f;
  layout.origin_y = kMargin + (area_height - bounds.height) / 2.0f;
  return layout;
}

Rectangle SidebarRect() {
  return {static_cast<float>(kScreenWidth) - kMargin - kSidebarWidth, kMargin,
          kSidebarWidth, static_cast<float>(kScreenHeight) - 2.0f * kMargin};
}

Rectangle ModeTabRect(Mode mode) {
  const float width = (InnerWidth() - 2.0f * 8.0f) / 3.0f;
  return {
      InnerLeft() + static_cast<float>(static_cast<int>(mode)) * (width + 8.0f),
      SidebarRect().y + 84.0f, width, 38.0f};
}

Rectangle ScorePanelRect() {
  const Rectangle tab = ModeTabRect(Mode::Moves);
  return {InnerLeft(), tab.y + tab.height + kGap + 6.0f, InnerWidth(), 112.0f};
}

Rectangle LimitPanelRect() {
  const Rectangle above = ScorePanelRect();
  return {InnerLeft(), above.y + above.height + kGap, InnerWidth(), 96.0f};
}

Rectangle BestPanelRect() {
  const Rectangle above = LimitPanelRect();
  return {InnerLeft(), above.y + above.height + kGap, InnerWidth(), 84.0f};
}

Rectangle NewGameButtonRect() {
  const Rectangle above = BestPanelRect();
  return {InnerLeft(), above.y + above.height + kGap + 6.0f, InnerWidth(),
          46.0f};
}

Rectangle HintButtonRect() {
  const Rectangle above = NewGameButtonRect();
  const float width = (InnerWidth() - 10.0f) / 2.0f;
  return {InnerLeft(), above.y + above.height + 10.0f, width, 38.0f};
}

Rectangle SoundButtonRect() {
  const Rectangle hint = HintButtonRect();
  return {hint.x + hint.width + 10.0f, hint.y, hint.width, hint.height};
}

}  // namespace match3
