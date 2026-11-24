#ifndef MATCH3_BOARD_H_
#define MATCH3_BOARD_H_

#include <cstdint>
#include <optional>
#include <random>
#include <string>
#include <vector>

#include "gem.h"

/// @brief Everything that belongs to the Match-3 game.
namespace match3 {

/// @brief Flat row-major cell storage: `cells[y * width + x]`.
///
/// Each cell is a `std::optional<Gem>`: it either holds a Gem or is empty
/// (`std::nullopt`). That is clearer than a sentinel value such as a fake
/// "no colour" GemColor, because the type itself says "maybe nothing here",
/// and reading an empty optional with `*` is easy to spot in review.
using Cells = std::vector<std::optional<Gem>>;

/// @brief Two neighbouring cells the player swaps.
struct Swap {
  /// @brief Cell the player picked up.
  GridPoint a;
  /// @brief Neighbouring cell the gem moved to.
  GridPoint b;
};

/// @brief A gem at a position, used to report clears and new specials.
struct PlacedGem {
  /// @brief Where the gem is (or was).
  GridPoint at;
  /// @brief The gem itself.
  Gem gem;
};

/// @brief What happens when the player swaps two special gems together.
///
/// A combo swap is always legal, even when it makes no match, and is much
/// stronger than the two specials going off one after the other.
enum class Combo : std::uint8_t {
  /// @brief Not a combo swap.
  None = 0,
  /// @brief Line + Line: clears the whole row and the whole column.
  Cross,
  /// @brief Line + Bomb: clears three rows and three columns.
  BigCross,
  /// @brief Bomb + Bomb: clears the 5x5 square around the swap.
  MegaBomb,
  /// @brief Rainbow + Line: every gem of the Line's colour becomes a Line
  /// gem, and they all go off.
  RainbowLine,
  /// @brief Rainbow + Bomb: every gem of the Bomb's colour becomes a Bomb,
  /// and they all go off.
  RainbowBomb,
  /// @brief Rainbow + Rainbow: clears the whole board.
  DoubleRainbow,
};

/// @brief Returns the combo that swapping `a` with `b` sets off.
///
/// The order of `a` and `b` does not matter. A Rainbow with a plain gem is
/// not a combo: it is the ordinary Rainbow swap.
/// @param a One of the two swapped gems.
/// @param b The other gem.
/// @return The combo, or `Combo::None` unless both gems are specials.
Combo ComboFor(const Gem& a, const Gem& b);

/// @brief One special gem going off, so the game can draw its effect.
struct Blast {
  /// @brief Where the special gem was.
  GridPoint at;
  /// @brief What kind of special it was.
  Special special = Special::None;
  /// @brief For a Bomb: how many cells the blast reaches from its centre
  /// (`1` = the usual 3x3 square, `2` = the 5x5 Mega Bomb). Unused for the
  /// other specials.
  int reach = 1;
};

/// @brief Everything one round of clearing did. The Game uses it for score,
/// sounds, and effects; the Board has already applied it.
struct ClearStep {
  /// @brief Gems removed from the board this step.
  std::vector<PlacedGem> cleared;
  /// @brief Special gems made by matches of 4, 5, or an L/T shape.
  std::vector<PlacedGem> created;
  /// @brief Special gems that went off, in the order they went off.
  std::vector<Blast> blasts;
  /// @brief The combo the player's swap set off (only ever on step 1).
  Combo combo = Combo::None;
  /// @brief 1 for the clear the player's swap made, 2 for the first cascade
  /// after it, and so on.
  int cascade = 1;
  /// @brief Points this step is worth (already multiplied by `cascade`).
  int points = 0;
};

/// @brief One gem sliding down after a clear, or dropping in from the top.
struct FallMove {
  /// @brief Cell the gem ends up in.
  GridPoint to;
  /// @brief Row the gem starts from, in cells. Negative for new gems, which
  /// start stacked above the top of the board.
  float from_y = 0.0f;
  /// @brief The gem that moves.
  Gem gem;
  /// @brief True for a new gem that dropped in from above the board.
  bool spawned = false;
};

/// @brief Totals from running every cascade to the end (see
/// `Board::ResolveAll()`).
struct CascadeResult {
  /// @brief Points scored across every step.
  int points = 0;
  /// @brief Number of clear steps (1 = no cascades after the swap).
  int steps = 0;
  /// @brief Gems cleared across every step.
  int cleared = 0;
};

/// @brief Returns the points for clearing `gems` gems in cascade step
/// `cascade`: 10 points per gem, multiplied by the cascade number.
/// @param gems Number of gems cleared.
/// @param cascade Cascade number (1 = the swap's own clear).
/// @return Points earned.
int ScoreFor(int gems, int cascade);

/// @brief Returns the bonus points for making a special gem.
/// @param special Kind of special gem made.
/// @return Bonus points (`0` for `Special::None`).
int SpecialBonus(Special special);

/// @brief The Match-3 rules: filling, swapping, matching, special gems,
/// gravity, and shuffling.
///
/// The Board resolves moves in small steps on purpose. After a swap, the
/// Game calls `ClearMatches()` and `Collapse()` in turn until no matches are
/// left, and animates between the calls. Tests and the hint finder can call
/// `ResolveAll()` to run every step at once.
///
/// New gems come from a seeded `std::mt19937`, so a seed always produces
/// the same game (handy for tests).
class Board {
 public:
  /// @brief Board width and height used by the game.
  static constexpr int kDefaultSize = 8;
  /// @brief Smallest allowed width or height.
  static constexpr int kMinSize = 3;
  /// @brief Largest allowed width or height.
  static constexpr int kMaxSize = 16;

  /// @brief Creates an 8x8 board seeded from system entropy.
  Board();

  /// @brief Creates and fills a board with a fixed seed.
  /// @param width Number of columns (clamped to `kMinSize..kMaxSize`).
  /// @param height Number of rows (clamped to `kMinSize..kMaxSize`).
  /// @param seed 32-bit PRNG seed for every gem the board makes.
  Board(int width, int height, std::uint32_t seed);

  /// @brief Refills the whole board with new gems. The new board never has
  /// a match on it, and always has at least one legal swap.
  /// @param width Number of columns (clamped to `kMinSize..kMaxSize`).
  /// @param height Number of rows (clamped to `kMinSize..kMaxSize`).
  /// @param seed 32-bit PRNG seed for every gem the board makes.
  void Reset(int width, int height, std::uint32_t seed);

  /// @brief Replaces the board with an exact layout, for tests and puzzles.
  ///
  /// Each string is one row, top row first. The letters `R O Y G B P` place
  /// a plain gem of that colour; any other character leaves the cell empty.
  /// The width is the length of the first row (missing characters in a
  /// shorter row count as empty). Sizes outside `kMinSize..kMaxSize` are
  /// clamped. The PRNG seed is kept.
  /// @param rows The layout, top row first.
  void SetLayout(const std::vector<std::string>& rows);

  /// @brief Puts `gem` in cell `at`, or empties it with `std::nullopt`.
  /// Does nothing outside the board.
  /// @param at Target cell.
  /// @param gem Gem to place, or `std::nullopt` to empty the cell.
  void SetGem(GridPoint at, std::optional<Gem> gem);

  /// @brief Swaps two neighbouring gems if the swap is legal.
  ///
  /// A swap is legal if it makes a match of 3 or more, if one of the two
  /// gems is a Rainbow, or if both gems are specials (a combo, see `Combo`).
  /// An illegal swap leaves the board unchanged, so the
  /// Game can animate the gems bouncing back.
  /// @param a Cell the player picked up.
  /// @param b Neighbouring cell (up, down, left, or right of `a`).
  /// @return True if the swap was made. The next `ClearMatches()` call then
  ///         clears it.
  bool ApplySwap(GridPoint a, GridPoint b);

  /// @brief Checks whether swapping `a` and `b` would be legal, without
  /// changing the board.
  /// @param a First cell.
  /// @param b Neighbouring cell.
  /// @return True if `ApplySwap(a, b)` would succeed.
  bool IsLegalSwap(GridPoint a, GridPoint b) const;

  /// @brief Clears every match on the board, sets off the special gems that
  /// get caught up in it, and puts new special gems where big matches were.
  ///
  /// It leaves holes where the gems were. Call `Collapse()` next to let the
  /// gems above fall into them.
  /// @return What was cleared, or `std::nullopt` if there was no match (the
  ///         move is finished).
  std::optional<ClearStep> ClearMatches();

  /// @brief Lets gems fall into the holes below them and drops new random
  /// gems in from the top, so every cell is full again.
  /// @return Every gem that moved or appeared.
  std::vector<FallMove> Collapse();

  /// @brief Calls `ClearMatches()` and `Collapse()` until nothing matches.
  /// @return Totals across every step.
  CascadeResult ResolveAll();

  /// @brief Rearranges the gems already on the board (specials included)
  /// until no match is on the board and at least one legal swap exists.
  /// If that fails many times in a row, the board is filled with new gems.
  void Shuffle();

  /// @brief Returns true if any match of 3 or more is on the board now.
  /// @return True when `ClearMatches()` would clear something.
  bool HasMatches() const;

  /// @brief Returns true if at least one legal swap exists.
  /// @return True when the player can still move.
  bool HasLegalMove() const;

  /// @brief Finds a legal swap to show the player as a hint.
  /// @return The first legal swap in reading order, or `std::nullopt`.
  std::optional<Swap> FindHint() const;

  /// @brief Returns true if `at` lies inside the board.
  /// @param at Cell coordinate.
  /// @return True for a valid cell.
  bool InBounds(GridPoint at) const;

  /// @brief Reads the cell at `at`.
  /// @param at Cell coordinate.
  /// @return The cell's gem, or an empty optional if the cell is empty or
  ///         out of bounds. The reference stays valid until the board next
  ///         changes size.
  const std::optional<Gem>& At(GridPoint at) const;

  /// @brief Number of columns.
  int width() const { return width_; }
  /// @brief Number of rows.
  int height() const { return height_; }
  /// @brief Number of clear steps since the last swap (0 before the first
  /// `ClearMatches()`).
  int cascade() const { return cascade_; }

 private:
  /// @brief Converts `at` to an index into `cells_`.
  /// @param at Cell coordinate (must be in bounds).
  /// @return Row-major index `y * width + x`.
  int Index(GridPoint at) const { return at.y * width_ + at.x; }

  /// @brief Fills every cell with random gems that make no match, retrying
  /// until at least one legal swap exists.
  void Fill();

  /// @brief Returns a uniformly random gem colour from `rng_`.
  /// @return One of the six colours.
  GemColor RandomColor();

  /// @brief Number of columns.
  int width_ = kDefaultSize;
  /// @brief Number of rows.
  int height_ = kDefaultSize;
  /// @brief Cell storage (see `Cells`).
  Cells cells_;
  /// @brief The swap that `ClearMatches()` should handle next, if any. It
  /// decides where new special gems appear and sets off Rainbow swaps.
  std::optional<Swap> pending_swap_;
  /// @brief Clear steps since the last swap.
  int cascade_ = 0;
  /// @brief Mersenne Twister PRNG used for every new gem and shuffle.
  std::mt19937 rng_;
};

}  // namespace match3

#endif  // MATCH3_BOARD_H_
