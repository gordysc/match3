#ifndef MATCH3_STORAGE_H_
#define MATCH3_STORAGE_H_

#include <array>
#include <filesystem>

#include "mode.h"

/// @brief Everything that belongs to the Match-3 game.
namespace match3 {

/// @brief Results recorded for one mode.
struct ModeRecord {
  /// @brief Highest final score (`0` = no finished game yet).
  int best_score = 0;
  /// @brief Number of games played to the end. Restarts don't count.
  int games = 0;
  /// @brief Longest cascade chain ever reached in this mode.
  int best_cascade = 0;
};

/// @brief Everything persisted between runs.
struct SaveData {
  /// @brief Mode selected when the game last closed.
  Mode mode = Mode::Moves;
  /// @brief True if the player turned the sound off.
  bool muted = false;
  /// @brief Records indexed by `static_cast<int>(Mode)`.
  std::array<ModeRecord, kModeCount> records{};
};

/// @brief Returns the default save file path (`~/.match3.cfg`).
/// @return `$HOME/.match3.cfg`, or `.match3.cfg` in the working directory if
///         `HOME` is unset.
std::filesystem::path DefaultSavePath();

/// @brief Loads SaveData from `path`. Missing or malformed fields fall back
/// to defaults without failing or throwing.
/// @param path File path to read.
/// @return Loaded SaveData (or defaults if the file does not exist yet).
SaveData LoadSaveData(const std::filesystem::path& path);

/// @brief Writes `data` to `path` in a human-readable `key=value` format.
/// @param path Destination file path (created or overwritten).
/// @param data Settings and records to persist.
/// @return True if the file was written cleanly.
bool WriteSaveData(const std::filesystem::path& path, const SaveData& data);

/// @brief Records a finished game and reports whether it set a new best
/// score.
/// @param data SaveData modified in place (passed by non-const `&`).
/// @param mode Mode the game was played in.
/// @param score Final score.
/// @param best_cascade Longest cascade chain reached in the game.
/// @return True if `score` beat (or set the first) best score.
bool RecordResult(SaveData& data, Mode mode, int score, int best_cascade);

}  // namespace match3

#endif  // MATCH3_STORAGE_H_
