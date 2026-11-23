#ifndef MATCH3_THEME_H_
#define MATCH3_THEME_H_

#include <raylib.h>

#include <array>

#include "gem.h"

/// @brief Everything that belongs to the Match-3 game.
namespace match3 {

// raylib's `Color` is a plain C struct of four `unsigned char`s, which makes
// it a "literal type": it can be built at compile time, so each colour here
// is an `inline constexpr` constant with no runtime cost.

/// @brief Window background behind every panel.
inline constexpr Color kBackground{16, 14, 28, 255};
/// @brief Lighter glow at the top of the background gradient.
inline constexpr Color kBackgroundTop{38, 28, 64, 255};
/// @brief Sidebar panel fill.
inline constexpr Color kPanel{30, 27, 50, 255};
/// @brief Panel fill under the mouse pointer.
inline constexpr Color kPanelHover{44, 40, 72, 255};
/// @brief Frame around the board.
inline constexpr Color kBoardFrame{26, 23, 44, 255};
/// @brief Light squares of the board's checker pattern.
inline constexpr Color kCellLight{44, 40, 70, 255};
/// @brief Dark squares of the board's checker pattern.
inline constexpr Color kCellDark{37, 33, 60, 255};
/// @brief Cell under the mouse pointer.
inline constexpr Color kCellHover{60, 55, 94, 255};

/// @brief Main text colour.
inline constexpr Color kText{238, 236, 250, 255};
/// @brief Secondary text colour for labels and hints.
inline constexpr Color kTextMuted{140, 134, 175, 255};
/// @brief Highlight for the selected mode tab and the selected gem.
inline constexpr Color kAccent{167, 139, 250, 255};
/// @brief Keyboard cursor outline.
inline constexpr Color kCursor{250, 204, 21, 255};
/// @brief Hint outline.
inline constexpr Color kHint{125, 211, 252, 255};
/// @brief "New best" text and cascade banners.
inline constexpr Color kSuccess{74, 222, 128, 255};
/// @brief Low-time and out-of-moves warnings.
inline constexpr Color kDanger{248, 113, 113, 255};
/// @brief Score digits.
inline constexpr Color kScoreDigits{253, 224, 71, 255};

/// @brief Base colour of each gem, indexed by `static_cast<int>(GemColor)`.
///
/// `std::array<Color, N>` instead of a C array `Color[N]`: it carries its
/// size, can be `constexpr`, and can be passed around like any value.
inline constexpr std::array<Color, kGemColorCount> kGemColors{{
    {239, 68, 68, 255},   // Red.
    {249, 145, 22, 255},  // Orange.
    {250, 214, 30, 255},  // Yellow.
    {52, 211, 103, 255},  // Green.
    {59, 130, 246, 255},  // Blue.
    {168, 85, 247, 255},  // Purple.
}};

/// @brief Returns the base colour of `color`.
/// @param color Gem colour.
/// @return The matching raylib Color.
inline Color GemTint(GemColor color) {
  return kGemColors[static_cast<std::size_t>(color)];
}

}  // namespace match3

#endif  // MATCH3_THEME_H_
