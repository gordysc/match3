#ifndef MATCH3_GEM_H_
#define MATCH3_GEM_H_

#include <cstdint>

/// @brief Everything that belongs to the Match-3 game.
namespace match3 {

/// @brief A column/row coordinate on the board. `(0, 0)` is the top-left cell.
struct GridPoint {
  /// @brief Column index.
  int x = 0;
  /// @brief Row index (grows downwards).
  int y = 0;

  /// @brief Defaulted equality (C++20) compares `x` and `y` member by member.
  bool operator==(const GridPoint&) const = default;
};

/// @brief The six gem colours. Each colour also has its own shape on screen,
/// so the game stays playable for colour-blind players.
///
/// `enum class` keeps the names scoped (`GemColor::Red`) and stops them from
/// turning into `int`s by accident. `: std::uint8_t` makes each value one
/// byte instead of the default `int`.
enum class GemColor : std::uint8_t {
  /// @brief Red circle.
  Red = 0,
  /// @brief Orange square.
  Orange,
  /// @brief Yellow diamond.
  Yellow,
  /// @brief Green triangle.
  Green,
  /// @brief Blue hexagon.
  Blue,
  /// @brief Purple pentagon.
  Purple,
};

/// @brief Number of GemColor values, for sizing per-colour arrays and for
/// picking a random colour.
inline constexpr int kGemColorCount = 6;

/// @brief Power a gem carries, earned by matching more than three.
enum class Special : std::uint8_t {
  /// @brief An ordinary gem.
  None = 0,
  /// @brief Made from a horizontal match of 4. Clears its whole row when it
  /// is cleared.
  LineH,
  /// @brief Made from a vertical match of 4. Clears its whole column when it
  /// is cleared.
  LineV,
  /// @brief Made from an L or T shape. Clears the 3x3 square around it.
  Bomb,
  /// @brief Made from a straight match of 5. Has no colour of its own: swap
  /// it with any gem to clear every gem of that gem's colour.
  Rainbow,
};

/// @brief One gem on the board.
struct Gem {
  /// @brief Colour (ignored for `Special::Rainbow`, which matches nothing).
  GemColor color = GemColor::Red;
  /// @brief Power the gem carries.
  Special special = Special::None;

  /// @brief Defaulted equality compares colour and special.
  bool operator==(const Gem&) const = default;
};

}  // namespace match3

#endif  // MATCH3_GEM_H_
