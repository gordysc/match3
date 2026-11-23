#ifndef MATCH3_MODE_H_
#define MATCH3_MODE_H_

#include <cstdint>

/// @brief Everything that belongs to the Match-3 game.
namespace match3 {

/// @brief The three ways to play.
enum class Mode : std::uint8_t {
  /// @brief A fixed number of swaps; score as much as you can with them.
  Moves = 0,
  /// @brief A fixed amount of time; swap as fast as you can.
  Blitz = 1,
  /// @brief No limit at all. Play until you get bored.
  Zen = 2,
};

/// @brief Number of Mode values, for sizing per-mode arrays.
inline constexpr int kModeCount = 3;

/// @brief The limits that make one mode different from another.
struct ModeRules {
  /// @brief Swaps allowed per game (`0` = no move limit).
  int moves = 0;
  /// @brief Seconds allowed per game (`0.0` = no time limit).
  float seconds = 0.0f;
};

/// @brief Returns the move and time limits for `mode`.
/// @param mode Mode to look up.
/// @return Matching ModeRules.
ModeRules RulesFor(Mode mode);

/// @brief Returns the upper-case display name of `mode`.
/// @param mode Mode to name.
/// @return Pointer to a static string literal (never free it).
const char* ModeName(Mode mode);

/// @brief Returns the lower-case key used for `mode` in the save file.
/// @param mode Mode to name.
/// @return Pointer to a static string literal such as `"blitz"`.
const char* ModeKey(Mode mode);

}  // namespace match3

#endif  // MATCH3_MODE_H_
