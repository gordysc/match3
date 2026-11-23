#include "mode.h"

/// @brief Everything that belongs to the Match-3 game.
namespace match3 {

ModeRules RulesFor(Mode mode) {
  // A `switch` over an `enum class` with no `default:` lets the compiler warn
  // (-Wswitch, part of -Wall) if a new Mode is added but not handled here.
  switch (mode) {
    case Mode::Moves:
      return {30, 0.0f};
    case Mode::Blitz:
      return {0, 90.0f};
    case Mode::Zen:
      return {0, 0.0f};
  }
  // Only reachable if someone casts an out-of-range number to Mode.
  return {};
}

const char* ModeName(Mode mode) {
  switch (mode) {
    case Mode::Moves:
      return "MOVES";
    case Mode::Blitz:
      return "BLITZ";
    case Mode::Zen:
      return "ZEN";
  }
  return "?";
}

const char* ModeKey(Mode mode) {
  switch (mode) {
    case Mode::Moves:
      return "moves";
    case Mode::Blitz:
      return "blitz";
    case Mode::Zen:
      return "zen";
  }
  return "unknown";
}

}  // namespace match3
