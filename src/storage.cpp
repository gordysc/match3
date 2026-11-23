#include "storage.h"

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <string>

/// @brief Everything that belongs to the Match-3 game.
namespace match3 {

/// @brief Defensive parsing helpers internal to `storage.cpp`.
namespace {

/// @brief Parses an integer from `value` and clamps it to `[min_val, max_val]`.
///
/// `std::stoi` throws `std::invalid_argument` or `std::out_of_range` on bad
/// input; catching all exceptions guarantees a hand-edited or corrupted save
/// file can never crash startup.
///
/// @param value Raw string slice after `=`.
/// @param fallback Value returned if parsing fails.
/// @param min_val Minimum permitted value.
/// @param max_val Maximum permitted value.
/// @return Clamped integer result or `fallback`.
int ParseInt(const std::string& value, int fallback, int min_val, int max_val) {
  try {
    return std::clamp(std::stoi(value), min_val, max_val);
  } catch (...) {
    return fallback;
  }
}

/// @brief Largest count or score the loader accepts.
constexpr int kMaxStored = 999999999;

}  // namespace

std::filesystem::path DefaultSavePath() {
  // `if (init; cond)` (C++17) limits `home`'s scope to this `if` statement.
  // `std::getenv` returns a non-owning pointer into the process environment
  // block (never free it), or `nullptr` if HOME is unset.
  if (const char* home = std::getenv("HOME");
      home != nullptr && *home != '\0') {
    // `operator/` on `std::filesystem::path` joins two path pieces with the
    // platform's directory separator.
    return std::filesystem::path(home) / ".match3.cfg";
  }
  return std::filesystem::path(".match3.cfg");
}

SaveData LoadSaveData(const std::filesystem::path& path) {
  SaveData data;
  // RAII: `std::ifstream` opens the file in its constructor and automatically
  // closes the file handle in its destructor when this function returns.
  std::ifstream input(path);
  if (!input.is_open()) return data;

  std::string line;
  while (std::getline(input, line)) {
    if (line.empty() || line[0] == '#') continue;
    const std::size_t eq = line.find('=');
    if (eq == std::string::npos) continue;

    const std::string key = line.substr(0, eq);
    const std::string val = line.substr(eq + 1);

    if (key == "mode") {
      data.mode = static_cast<Mode>(ParseInt(val, 0, 0, kModeCount - 1));
      continue;
    }
    if (key == "muted") {
      data.muted = ParseInt(val, 0, 0, 1) == 1;
      continue;
    }

    // Per-mode keys look like `blitz_best_score`. Try each prefix.
    for (int i = 0; i < kModeCount; ++i) {
      const std::string prefix =
          std::string(ModeKey(static_cast<Mode>(i))) + "_";
      // C++20 `starts_with` is a clearer "begins with" test than the older
      // `key.rfind(prefix, 0) == 0` idiom.
      if (!key.starts_with(prefix)) continue;
      const std::string field = key.substr(prefix.size());
      // `records[i]` by reference, so the assignments below write into
      // `data` rather than into a temporary copy.
      ModeRecord& record = data.records[static_cast<std::size_t>(i)];
      if (field == "best_score") {
        record.best_score = ParseInt(val, 0, 0, kMaxStored);
      } else if (field == "games") {
        record.games = ParseInt(val, 0, 0, kMaxStored);
      } else if (field == "best_cascade") {
        record.best_cascade = ParseInt(val, 0, 0, kMaxStored);
      }
    }
  }
  return data;
}

bool WriteSaveData(const std::filesystem::path& path, const SaveData& data) {
  // `std::ofstream` truncates (empties) an existing file on open by default.
  std::ofstream output(path);
  if (!output.is_open()) return false;

  output << "# Match-3 save file\n";
  output << "mode=" << static_cast<int>(data.mode) << "\n";
  output << "muted=" << (data.muted ? 1 : 0) << "\n";
  for (int i = 0; i < kModeCount; ++i) {
    const std::string key = ModeKey(static_cast<Mode>(i));
    const ModeRecord& record = data.records[static_cast<std::size_t>(i)];
    output << key << "_best_score=" << record.best_score << "\n";
    output << key << "_games=" << record.games << "\n";
    output << key << "_best_cascade=" << record.best_cascade << "\n";
  }
  // `good()` is false if any write above failed (disk full, permissions).
  return output.good();
}

bool RecordResult(SaveData& data, Mode mode, int score, int best_cascade) {
  ModeRecord& record = data.records[static_cast<std::size_t>(mode)];
  ++record.games;
  record.best_cascade = std::max(record.best_cascade, best_cascade);
  if (score > record.best_score) {
    record.best_score = score;
    return true;
  }
  return false;
}

}  // namespace match3
