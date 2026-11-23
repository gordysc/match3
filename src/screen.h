// Header guard: ensures this header's definitions are included at most once
// per translation unit (.cpp file), preventing duplicate-definition errors.
#ifndef MATCH3_SCREEN_H_
#define MATCH3_SCREEN_H_

/// @brief Everything that belongs to the Match-3 game.
namespace match3 {

// `inline constexpr` (C++17) on a header variable gives it external linkage
// across every .cpp file that includes this header while still remaining a
// compile-time constant. Without `inline`, each translation unit would get its
// own separate internal copy.

/// @brief Window width in logical pixels.
inline constexpr int kScreenWidth = 1080;
/// @brief Window height in logical pixels.
inline constexpr int kScreenHeight = 760;

}  // namespace match3

#endif  // MATCH3_SCREEN_H_
