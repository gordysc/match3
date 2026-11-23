#include <array>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <string>
#include <system_error>
#include <vector>

#include "board.h"
#include "mode.h"
#include "storage.h"

/// @brief Minimal self-contained unit test suite for `match3_core`.
namespace {

// `using` declarations bring a few names into this namespace so the tests
// can write `Board` instead of `match3::Board`.
using match3::Board;
using match3::Gem;
using match3::GemColor;
using match3::GridPoint;
using match3::Special;

/// @brief Total number of assertions that failed during this test run.
int g_failures = 0;

/// @brief Checks a boolean condition and prints a diagnostic message on
/// failure.
/// @param condition Expression expected to be true.
/// @param message Human-readable failure description.
void Expect(bool condition, const char* message) {
  if (!condition) {
    std::cerr << "FAIL: " << message << "\n";
    ++g_failures;
  }
}

/// @brief Returns true if the cell at `(x, y)` is empty.
/// @param board Board to read (by `const&`: read-only, no copy).
/// @param x Column index.
/// @param y Row index.
/// @return True when the cell holds no gem.
bool IsEmpty(const Board& board, int x, int y) {
  return !board.At({x, y}).has_value();
}

/// @brief Returns true if the cell at `(x, y)` holds exactly `gem`.
/// @param board Board to read.
/// @param x Column index.
/// @param y Row index.
/// @param gem Expected gem.
/// @return True on a match.
bool Holds(const Board& board, int x, int y, Gem gem) {
  // Comparing an optional with a plain value is true only when the optional
  // holds a value and that value compares equal.
  return board.At({x, y}) == gem;
}

/// @brief Returns true if every cell on the board holds a gem.
/// @param board Board to scan.
/// @return True when no cell is empty.
bool IsFull(const Board& board) {
  for (int y = 0; y < board.height(); ++y) {
    for (int x = 0; x < board.width(); ++x) {
      if (IsEmpty(board, x, y)) return false;
    }
  }
  return true;
}

/// @brief Counts the gems of each colour (specials included).
/// @param board Board to scan.
/// @return Count per colour, indexed by `static_cast<int>(GemColor)`.
std::array<int, match3::kGemColorCount> CountColors(const Board& board) {
  std::array<int, match3::kGemColorCount> counts{};
  for (int y = 0; y < board.height(); ++y) {
    for (int x = 0; x < board.width(); ++x) {
      if (const auto& cell = board.At({x, y})) {
        ++counts[static_cast<std::size_t>(cell->color)];
      }
    }
  }
  return counts;
}

/// @brief Verifies new boards are full, have no match, have a legal move,
/// and are the same for the same seed.
void TestFill() {
  for (std::uint32_t seed = 1; seed <= 200; ++seed) {
    const Board board(Board::kDefaultSize, Board::kDefaultSize, seed);
    Expect(IsFull(board), "A new board has a gem in every cell");
    Expect(!board.HasMatches(), "A new board has no match on it");
    Expect(board.HasLegalMove(), "A new board has a legal move");
  }

  const Board a(8, 8, 42);
  const Board b(8, 8, 42);
  bool same = true;
  for (int y = 0; y < 8; ++y) {
    for (int x = 0; x < 8; ++x) same = same && a.At({x, y}) == b.At({x, y});
  }
  Expect(same, "The same seed makes the same board");

  const Board tiny(1, 99, 1);
  Expect(tiny.width() == Board::kMinSize && tiny.height() == Board::kMaxSize,
         "Board sizes are clamped to kMinSize..kMaxSize");
}

/// @brief Verifies illegal swaps are rejected and leave the board alone.
void TestIllegalSwaps() {
  Board board;
  board.SetLayout({"RGR....."});
  Expect(!board.ApplySwap({0, 0}, {1, 0}), "A swap that makes no match fails");
  Expect(Holds(board, 0, 0, Gem{GemColor::Red}) &&
             Holds(board, 1, 0, Gem{GemColor::Green}),
         "A failed swap leaves the gems where they were");
  Expect(!board.ApplySwap({0, 0}, {2, 0}),
         "Cells that are not neighbours fail");
  Expect(!board.ApplySwap({0, 0}, {1, 1}), "Diagonal swaps fail");
  Expect(!board.ApplySwap({0, 0}, {0, 1}), "Swaps with an empty cell fail");
  Expect(!board.ApplySwap({0, 0}, {-1, 0}), "Swaps off the board fail");
  Expect(!board.ClearMatches().has_value(),
         "Nothing clears after a failed swap");
}

/// @brief Verifies a plain match of 3 clears, scores, and makes no special.
void TestMatchOfThree() {
  Board board;
  board.SetLayout({"RRGR...."});
  Expect(board.IsLegalSwap({2, 0}, {3, 0}), "IsLegalSwap sees the match");
  Expect(board.ApplySwap({2, 0}, {3, 0}), "Swapping G and R makes RRR");
  const auto step = board.ClearMatches();
  Expect(step.has_value(), "The match clears");
  if (!step) return;
  Expect(step->cleared.size() == 3, "Three gems clear");
  Expect(step->created.empty(), "A match of 3 makes no special gem");
  Expect(step->cascade == 1 && step->points == 30, "A match of 3 scores 30");
  Expect(IsEmpty(board, 0, 0) && IsEmpty(board, 1, 0) && IsEmpty(board, 2, 0),
         "The matched cells are empty");
  Expect(Holds(board, 3, 0, Gem{GemColor::Green}), "The other gem stays");
  Expect(!board.ClearMatches().has_value(), "Nothing is left to clear");
}

/// @brief Verifies 4 in a line makes a Line gem on the moved gem's cell.
void TestMatchOfFour() {
  Board board;
  board.SetLayout({"RRGR....", "..R....."});
  Expect(board.ApplySwap({2, 1}, {2, 0}), "Moving R up makes RRRR");
  auto step = board.ClearMatches();
  Expect(step && step->cleared.size() == 4, "Four gems clear");
  Expect(step && step->points == 4 * 10 + 50, "A Line gem earns a bonus");
  Expect(Holds(board, 2, 0, Gem{GemColor::Red, Special::LineH}),
         "A horizontal 4 makes a LineH where the gem moved to");
  Expect(IsEmpty(board, 0, 0) && IsEmpty(board, 3, 0),
         "The rest of the match is gone");

  board.SetLayout({"R.......", "R.......", "GR......", "R......."});
  Expect(board.ApplySwap({1, 2}, {0, 2}), "Moving R left makes a column of 4");
  step = board.ClearMatches();
  Expect(Holds(board, 0, 2, Gem{GemColor::Red, Special::LineV}),
         "A vertical 4 makes a LineV");
}

/// @brief Verifies 5 in a line makes a Rainbow and an L shape makes a Bomb.
void TestMatchOfFiveAndShapes() {
  Board board;
  board.SetLayout({"RRGRR...", "..R....."});
  Expect(board.ApplySwap({2, 1}, {2, 0}), "Moving R up makes RRRRR");
  auto step = board.ClearMatches();
  Expect(step && step->created.size() == 1, "One special gem is made");
  Expect(board.At({2, 0}) && board.At({2, 0})->special == Special::Rainbow,
         "5 in a line makes a Rainbow");

  board.SetLayout({"R.......", "R.......", "GRR.....", "R......."});
  Expect(board.ApplySwap({0, 3}, {0, 2}), "Moving R up makes an L shape");
  step = board.ClearMatches();
  Expect(step && step->cleared.size() == 5, "The L clears 5 gems");
  Expect(step && step->points == 5 * 10 + 100, "A Bomb earns a bonus");
  Expect(Holds(board, 0, 2, Gem{GemColor::Red, Special::Bomb}),
         "An L shape makes a Bomb where the gem moved to");
}

/// @brief Verifies Line gems clear their row or column.
void TestLineBlasts() {
  Board board;
  board.SetLayout({"........", "........", "........", "RRRBGYOP"});
  board.SetGem({0, 3}, Gem{GemColor::Red, Special::LineH});
  auto step = board.ClearMatches();
  Expect(step && step->cleared.size() == 8, "A LineH clears its whole row");
  Expect(step && step->blasts.size() == 1 &&
             step->blasts[0].special == Special::LineH,
         "The LineH blast is reported");

  board.SetLayout({"..O.....", "..Y.....", "..B.....", "..G.....", "RRR.....",
                   "..P.....", "..O.....", "..Y....."});
  board.SetGem({2, 4}, Gem{GemColor::Red, Special::LineV});
  step = board.ClearMatches();
  Expect(step && step->cleared.size() == 10,
         "A LineV clears its column plus the rest of the match");
  for (int y = 0; y < 8; ++y) {
    Expect(IsEmpty(board, 2, y), "Every cell in the column is empty");
  }
}

/// @brief Verifies a Bomb clears 3x3 and sets off specials it catches.
void TestBombChain() {
  Board board;
  board.SetLayout({"........", "OYBG....", "RRRY....", "GBOP...."});
  board.SetGem({1, 2}, Gem{GemColor::Red, Special::Bomb});
  board.SetGem({2, 3}, Gem{GemColor::Orange, Special::LineH});
  const auto step = board.ClearMatches();
  Expect(step.has_value(), "The match with the Bomb clears");
  if (!step) return;
  Expect(step->blasts.size() == 2, "The Bomb sets off the LineH");
  Expect(step->cleared.size() == 10,
         "Bomb (3x3) plus LineH (rest of row 3) clear 10 gems");
  Expect(Holds(board, 3, 1, Gem{GemColor::Green}) &&
             Holds(board, 3, 2, Gem{GemColor::Yellow}),
         "Gems outside both blasts stay");
  Expect(IsEmpty(board, 3, 3), "The LineH reached the end of its row");
}

/// @brief Verifies Rainbow swaps clear every gem of the other colour.
void TestRainbowSwap() {
  Board board;
  board.SetLayout({"RGBRGBP.", "BRGYOP.."});
  board.SetGem({6, 0}, Gem{GemColor::Red, Special::Rainbow});
  Expect(board.IsLegalSwap({6, 0}, {5, 0}),
         "A Rainbow swap is legal without a match");
  Expect(board.ApplySwap({6, 0}, {5, 0}), "The Rainbow swaps with a blue gem");
  const auto step = board.ClearMatches();
  Expect(step && step->cleared.size() == 4,
         "Three blue gems and the Rainbow clear");
  Expect(IsEmpty(board, 2, 0) && IsEmpty(board, 0, 1) && IsEmpty(board, 6, 0),
         "Every blue gem is gone");
  Expect(Holds(board, 0, 0, Gem{GemColor::Red}), "Other colours stay");

  board.SetLayout({"RGB.....", "OYP....."});
  board.SetGem({0, 0}, Gem{GemColor::Red, Special::Rainbow});
  board.SetGem({1, 0}, Gem{GemColor::Green, Special::Rainbow});
  Expect(board.ApplySwap({0, 0}, {1, 0}), "Two Rainbows can swap");
  const auto both = board.ClearMatches();
  Expect(both && both->cleared.size() == 6,
         "Two Rainbows clear every gem on the board");
}

/// @brief Verifies gravity moves gems down in order and refills the top.
void TestCollapse() {
  Board board;
  board.SetLayout({"R.......", "G.......", "........", "B.......", "........",
                   "........", "........", "........"});
  const std::vector<match3::FallMove> moves = board.Collapse();
  Expect(IsFull(board), "Every cell is full after Collapse");
  Expect(Holds(board, 0, 7, Gem{GemColor::Blue}) &&
             Holds(board, 0, 6, Gem{GemColor::Green}) &&
             Holds(board, 0, 5, Gem{GemColor::Red}),
         "Gems fall to the bottom and keep their order");
  int spawned = 0;
  bool spawn_starts_above = true;
  for (const match3::FallMove& move : moves) {
    if (!move.spawned) continue;
    ++spawned;
    spawn_starts_above = spawn_starts_above && move.from_y < 0.0f;
  }
  Expect(spawned == 64 - 3, "Every empty cell gets a new gem");
  Expect(spawn_starts_above, "New gems start above the board");
  Expect(moves.size() == 64, "Three moved gems plus 61 new ones");
}

/// @brief Verifies each clear step after a swap scores more.
void TestCascadeMultiplier() {
  Board board;
  board.SetLayout({"RRR....."});
  auto step = board.ClearMatches();
  Expect(step && step->cascade == 1 && step->points == 30,
         "The first step scores x1");
  for (int x = 0; x < 3; ++x) board.SetGem({x, 1}, Gem{GemColor::Green});
  step = board.ClearMatches();
  Expect(step && step->cascade == 2 && step->points == 60,
         "The second step scores x2");
  Expect(board.cascade() == 2, "cascade() counts the steps");

  board.SetGem({0, 2}, Gem{GemColor::Blue});
  board.SetGem({1, 2}, Gem{GemColor::Blue});
  board.SetGem({2, 2}, Gem{GemColor::Yellow});
  board.SetGem({2, 1}, Gem{GemColor::Blue});
  Expect(board.ApplySwap({2, 1}, {2, 2}), "A new swap is legal");
  Expect(board.cascade() == 0, "A new swap resets the cascade count");
  step = board.ClearMatches();
  Expect(step && step->cascade == 1, "Its clear is step 1 again");
}

/// @brief Verifies shuffling keeps the same gems and fixes a dead board.
void TestShuffle() {
  // Colour (x + 2y) mod 4 leaves no swap that makes a match.
  std::vector<std::string> dead;
  const std::string letters = "RGBY";
  for (int y = 0; y < 8; ++y) {
    std::string row;
    for (int x = 0; x < 8; ++x) row += letters[(x + 2 * y) % 4];
    dead.push_back(row);
  }
  Board board(8, 8, 3);
  board.SetLayout(dead);
  Expect(!board.HasMatches(), "The dead board has no match");
  Expect(!board.HasLegalMove() && !board.FindHint(),
         "The dead board has no legal move");

  const auto before = CountColors(board);
  board.Shuffle();
  Expect(CountColors(board) == before, "Shuffle keeps the same gems");
  Expect(!board.HasMatches(), "Shuffle leaves no match");
  Expect(board.HasLegalMove(), "Shuffle leaves a legal move");
}

/// @brief Plays many games with the hint as the player and checks the board
/// is always full, settled, and playable.
void TestPlayWithHints() {
  for (std::uint32_t seed = 1; seed <= 20; ++seed) {
    Board board(8, 8, seed);
    for (int turn = 0; turn < 50; ++turn) {
      const auto hint = board.FindHint();
      Expect(hint.has_value(), "A settled board always has a hint");
      if (!hint) break;
      Expect(board.ApplySwap(hint->a, hint->b), "The hint is a legal swap");
      const match3::CascadeResult result = board.ResolveAll();
      Expect(result.steps >= 1 && result.points > 0, "The hint scores");
      Expect(IsFull(board), "The board is full after every move");
      Expect(!board.HasMatches(), "No match is left after ResolveAll");
      if (!board.HasLegalMove()) board.Shuffle();
    }
  }
}

/// @brief Verifies the mode presets and scoring helpers.
void TestModesAndScoring() {
  using match3::Mode;
  using match3::RulesFor;
  Expect(
      RulesFor(Mode::Moves).moves == 30 && RulesFor(Mode::Moves).seconds == 0,
      "Moves mode allows 30 swaps");
  Expect(
      RulesFor(Mode::Blitz).moves == 0 && RulesFor(Mode::Blitz).seconds == 90,
      "Blitz mode allows 90 seconds");
  Expect(RulesFor(Mode::Zen).moves == 0 && RulesFor(Mode::Zen).seconds == 0,
         "Zen mode has no limits");
  Expect(match3::ScoreFor(3, 1) == 30 && match3::ScoreFor(5, 3) == 150,
         "ScoreFor is 10 x gems x cascade");
  Expect(match3::SpecialBonus(Special::None) == 0 &&
             match3::SpecialBonus(Special::Rainbow) == 250,
         "SpecialBonus rewards bigger specials more");
}

/// @brief Verifies records round-trip through a save file on disk.
void TestStorageRoundTrip() {
  const std::filesystem::path temp_path =
      std::filesystem::temp_directory_path() / "match3_test.cfg";

  match3::SaveData out;
  out.mode = match3::Mode::Blitz;
  out.muted = true;
  Expect(match3::RecordResult(out, match3::Mode::Blitz, 4200, 5),
         "The first finished game sets a best score");
  Expect(!match3::RecordResult(out, match3::Mode::Blitz, 1000, 2),
         "A lower score is not a best");
  Expect(match3::WriteSaveData(temp_path, out), "WriteSaveData succeeds");

  const match3::SaveData in = match3::LoadSaveData(temp_path);
  Expect(in.mode == match3::Mode::Blitz, "mode round-trips");
  Expect(in.muted, "muted round-trips");
  Expect(in.records[1].best_score == 4200, "blitz_best_score round-trips");
  Expect(in.records[1].games == 2, "blitz_games round-trips");
  Expect(in.records[1].best_cascade == 5, "blitz_best_cascade round-trips");
  Expect(in.records[0].games == 0, "Other modes stay empty");

  // `std::error_code` overload: reports failure through `ec` instead of
  // throwing, so cleanup can never abort the test run.
  std::error_code ec;
  std::filesystem::remove(temp_path, ec);
}

}  // namespace

/// @brief Entry point for the CTest unit test binary.
/// @return 0 if all tests pass, non-zero otherwise.
int main() {
  TestFill();
  TestIllegalSwaps();
  TestMatchOfThree();
  TestMatchOfFour();
  TestMatchOfFiveAndShapes();
  TestLineBlasts();
  TestBombChain();
  TestRainbowSwap();
  TestCollapse();
  TestCascadeMultiplier();
  TestShuffle();
  TestPlayWithHints();
  TestModesAndScoring();
  TestStorageRoundTrip();

  if (g_failures > 0) {
    std::cerr << g_failures << " test assertion(s) failed.\n";
    return 1;
  }
  std::cout << "All match3_core tests passed!\n";
  return 0;
}
