#include "board.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdlib>
#include <numeric>

/// @brief Everything that belongs to the Match-3 game.
namespace match3 {

/// @brief Match-finding helpers internal to `board.cpp`.
///
/// They work on a plain `Cells` vector instead of on the Board, so the
/// legal-move search can try swaps on a scratch copy without touching the
/// real board.
namespace {

/// @brief The empty cell that `Board::At()` returns for out-of-bounds reads.
///
/// `At()` returns a reference, so it needs an object that outlives every
/// call. A namespace-scope constant lives for the whole program, so a
/// reference to it can never dangle. (Returning a reference to a local
/// variable would dangle as soon as the function returned.)
const std::optional<Gem> kEmptyCell;

/// @brief Returns true if `cell` holds a gem that can be part of a colour
/// match. Empty cells and Rainbow gems (which have no colour) cannot.
/// @param cell Cell to test.
/// @return True for a matchable gem.
bool Matchable(const std::optional<Gem>& cell) {
  // `->` on an optional reads a member of the value inside. It is only safe
  // after `has_value()` is true; `&&` stops before `->` runs if it is false.
  return cell.has_value() && cell->special != Special::Rainbow;
}

/// @brief Returns true if two cells hold matchable gems of the same colour.
/// @param a First cell.
/// @param b Second cell.
/// @return True when `a` and `b` would count towards the same match.
bool SameColor(const std::optional<Gem>& a, const std::optional<Gem>& b) {
  return Matchable(a) && Matchable(b) && a->color == b->color;
}

/// @brief Counts the same-coloured gems in a straight line through `at`,
/// along the axis `(dx, dy)` in both directions, including `at` itself.
/// @param cells Cell storage.
/// @param width Number of columns.
/// @param height Number of rows.
/// @param at Cell the line passes through.
/// @param dx `1` for a horizontal line, `0` for a vertical one.
/// @param dy `0` for a horizontal line, `1` for a vertical one.
/// @return Length of the line (`0` if `at` is not matchable).
int RunThrough(const Cells& cells, int width, int height, GridPoint at, int dx,
               int dy) {
  const std::optional<Gem>& centre = cells[at.y * width + at.x];
  if (!Matchable(centre)) return 0;
  int length = 1;
  // A braced list in a range-based `for` builds a temporary
  // `std::initializer_list<int>`, a quick way to loop over a few values.
  for (const int sign : {-1, 1}) {
    int x = at.x + dx * sign;
    int y = at.y + dy * sign;
    while (x >= 0 && x < width && y >= 0 && y < height &&
           SameColor(cells[y * width + x], centre)) {
      ++length;
      x += dx * sign;
      y += dy * sign;
    }
  }
  return length;
}

/// @brief Returns true if `at` is part of a match of 3 or more.
/// @param cells Cell storage.
/// @param width Number of columns.
/// @param height Number of rows.
/// @param at Cell to test.
/// @return True when a horizontal or vertical line through `at` has 3+.
bool MatchAt(const Cells& cells, int width, int height, GridPoint at) {
  return RunThrough(cells, width, height, at, 1, 0) >= 3 ||
         RunThrough(cells, width, height, at, 0, 1) >= 3;
}

/// @brief Swaps two cells, checks whether either one is now in a match, and
/// swaps them back.
///
/// `cells` is passed by non-const reference because the function changes it
/// for a moment. It always puts the cells back before it returns.
/// @param cells Scratch cell storage.
/// @param width Number of columns.
/// @param height Number of rows.
/// @param a First cell (in bounds).
/// @param b Second cell (in bounds).
/// @return True if the swap makes a match.
bool SwapMakesMatch(Cells& cells, int width, int height, GridPoint a,
                    GridPoint b) {
  auto& cell_a = cells[a.y * width + a.x];
  auto& cell_b = cells[b.y * width + b.x];
  // `std::swap` on two optionals swaps their contents (or their emptiness).
  std::swap(cell_a, cell_b);
  const bool match =
      MatchAt(cells, width, height, a) || MatchAt(cells, width, height, b);
  std::swap(cell_a, cell_b);
  return match;
}

/// @brief Returns true if swapping these two cells is legal even when it
/// makes no match: one gem is a Rainbow, or both gems are specials (a
/// combo).
/// @param a First cell.
/// @param b Second cell.
/// @return True for a Rainbow swap or a combo swap.
bool IsFreeSwap(const std::optional<Gem>& a, const std::optional<Gem>& b) {
  // Both cells must hold a gem: a swap with an empty cell is never legal.
  if (!a || !b) return false;
  return a->special == Special::Rainbow || b->special == Special::Rainbow ||
         ComboFor(*a, *b) != Combo::None;
}

/// @brief A straight line of 3 or more same-coloured gems.
struct Run {
  /// @brief Leftmost (horizontal) or topmost (vertical) cell.
  GridPoint start;
  /// @brief Number of gems in the line.
  int length = 0;
  /// @brief True for a row, false for a column.
  bool horizontal = true;

  /// @brief Returns the `i`-th cell of the run.
  /// @param i Index from `0` to `length - 1`.
  /// @return Cell coordinate.
  GridPoint CellAt(int i) const {
    return horizontal ? GridPoint{start.x + i, start.y}
                      : GridPoint{start.x, start.y + i};
  }
};

/// @brief Finds every run: all horizontal runs first, then all vertical.
/// @param cells Cell storage.
/// @param width Number of columns.
/// @param height Number of rows.
/// @return Every line of 3 or more.
std::vector<Run> FindRuns(const Cells& cells, int width, int height) {
  std::vector<Run> runs;
  for (int y = 0; y < height; ++y) {
    int x = 0;
    while (x < width) {
      const std::optional<Gem>& first = cells[y * width + x];
      int length = 1;
      while (x + length < width &&
             SameColor(cells[y * width + x + length], first)) {
        ++length;
      }
      if (Matchable(first) && length >= 3)
        runs.push_back({{x, y}, length, true});
      x += length;
    }
  }
  for (int x = 0; x < width; ++x) {
    int y = 0;
    while (y < height) {
      const std::optional<Gem>& first = cells[y * width + x];
      int length = 1;
      while (y + length < height &&
             SameColor(cells[(y + length) * width + x], first)) {
        ++length;
      }
      if (Matchable(first) && length >= 3) {
        runs.push_back({{x, y}, length, false});
      }
      y += length;
    }
  }
  return runs;
}

/// @brief Runs that touch each other, cleared together as one match. An L or
/// T shape is one group made of a horizontal and a vertical run.
struct MatchGroup {
  /// @brief Every cell in the group, without repeats.
  std::vector<GridPoint> cells;
  /// @brief Colour shared by every gem in the group.
  GemColor color = GemColor::Red;
  /// @brief The group's longest run.
  Run longest;
  /// @brief True if a horizontal and a vertical run cross (L or T shape).
  bool crossed = false;
  /// @brief A cell where the two runs cross, if `crossed`.
  GridPoint cross_cell;
};

/// @brief Finds the root of `i` in a union-find forest.
///
/// Union-find (also called a disjoint-set forest) stores groups as trees in
/// a `parent` array: two items are in the same group when they have the same
/// root. "Path halving" points each visited item at its grandparent, which
/// keeps the trees flat and the lookups fast.
/// @param parent Parent index of every item (roots point at themselves).
/// @param i Item to look up.
/// @return Index of the root of `i`'s group.
std::size_t FindRoot(std::vector<std::size_t>& parent, std::size_t i) {
  while (parent[i] != i) {
    parent[i] = parent[parent[i]];
    i = parent[i];
  }
  return i;
}

/// @brief Collects runs into groups: two runs that share a cell are one
/// group.
/// @param cells Cell storage.
/// @param width Number of columns.
/// @param height Number of rows.
/// @return One MatchGroup per separate match on the board.
std::vector<MatchGroup> FindGroups(const Cells& cells, int width, int height) {
  const std::vector<Run> runs = FindRuns(cells, width, height);
  const std::size_t cell_count = cells.size();

  // Start with every run in its own group. `std::iota` fills the vector with
  // 0, 1, 2, ..., so each run is its own root.
  std::vector<std::size_t> parent(runs.size());
  std::iota(parent.begin(), parent.end(), std::size_t{0});

  // A cell is in at most one horizontal and one vertical run. Remember which
  // (`-1` = none), and join a vertical run to any horizontal run it crosses.
  std::vector<int> h_owner(cell_count, -1);
  std::vector<int> v_owner(cell_count, -1);
  for (std::size_t r = 0; r < runs.size(); ++r) {
    for (int i = 0; i < runs[r].length; ++i) {
      const GridPoint p = runs[r].CellAt(i);
      const auto index = static_cast<std::size_t>(p.y * width + p.x);
      if (runs[r].horizontal) {
        h_owner[index] = static_cast<int>(r);
      } else {
        v_owner[index] = static_cast<int>(r);
        if (h_owner[index] >= 0) {
          const std::size_t a = FindRoot(parent, r);
          const std::size_t b =
              FindRoot(parent, static_cast<std::size_t>(h_owner[index]));
          parent[a] = b;
        }
      }
    }
  }

  std::vector<MatchGroup> groups;
  // Which group each root became (`-1` = not made yet).
  std::vector<int> group_of_root(runs.size(), -1);
  // `std::uint8_t` instead of `bool`: `std::vector<bool>` is a special
  // bit-packed container whose elements are not real `bool`s.
  std::vector<std::uint8_t> seen(cell_count, 0);
  for (std::size_t r = 0; r < runs.size(); ++r) {
    const std::size_t root = FindRoot(parent, r);
    if (group_of_root[root] < 0) {
      group_of_root[root] = static_cast<int>(groups.size());
      MatchGroup group;
      const GridPoint start = runs[r].start;
      group.color =
          cells[static_cast<std::size_t>(start.y * width + start.x)]->color;
      groups.push_back(group);
    }
    // Take the reference only after `push_back`: adding to a vector can move
    // its elements to a new buffer, which would leave an older reference
    // pointing at freed memory.
    MatchGroup& group = groups[static_cast<std::size_t>(group_of_root[root])];
    if (runs[r].length > group.longest.length) group.longest = runs[r];
    for (int i = 0; i < runs[r].length; ++i) {
      const GridPoint p = runs[r].CellAt(i);
      const auto index = static_cast<std::size_t>(p.y * width + p.x);
      if (h_owner[index] >= 0 && v_owner[index] >= 0) {
        group.crossed = true;
        group.cross_cell = p;
      }
      if (seen[index] == 0) {
        seen[index] = 1;
        group.cells.push_back(p);
      }
    }
  }
  return groups;
}

/// @brief Decides which special gem a match group earns.
/// @param group The match.
/// @return Rainbow for 5 in a line, Bomb for an L or T, a Line gem for 4 in a
///         line, or `Special::None` for a plain match of 3.
Special SpecialFor(const MatchGroup& group) {
  if (group.longest.length >= 5) return Special::Rainbow;
  if (group.crossed) return Special::Bomb;
  if (group.longest.length == 4) {
    return group.longest.horizontal ? Special::LineH : Special::LineV;
  }
  return Special::None;
}

/// @brief Returns true if `cells` contains `point`.
/// @param cells Cells to search.
/// @param point Cell to look for.
/// @return True if found.
bool Contains(const std::vector<GridPoint>& cells, GridPoint point) {
  // `std::ranges::find` (C++20) takes the whole container instead of a
  // begin/end iterator pair.
  return std::ranges::find(cells, point) != cells.end();
}

/// @brief Turns a layout letter into a colour.
/// @param letter One of `R O Y G B P`.
/// @return The colour, or `std::nullopt` for any other character.
std::optional<GemColor> ColorFromLetter(char letter) {
  switch (letter) {
    case 'R':
      return GemColor::Red;
    case 'O':
      return GemColor::Orange;
    case 'Y':
      return GemColor::Yellow;
    case 'G':
      return GemColor::Green;
    case 'B':
      return GemColor::Blue;
    case 'P':
      return GemColor::Purple;
    default:
      return std::nullopt;
  }
}

}  // namespace

int ScoreFor(int gems, int cascade) { return gems * 10 * std::max(1, cascade); }

Combo ComboFor(const Gem& a, const Gem& b) {
  // A lambda with an empty capture list `[]` uses nothing from the
  // enclosing scope, so it behaves like a small local function.
  const auto is_line = [](Special s) {
    return s == Special::LineH || s == Special::LineV;
  };
  const Special sa = a.special;
  const Special sb = b.special;
  if (sa == Special::None || sb == Special::None) return Combo::None;

  // From here on both gems are specials. Each pair is checked in both
  // orders, so `ComboFor(a, b) == ComboFor(b, a)`.
  if (sa == Special::Rainbow && sb == Special::Rainbow) {
    return Combo::DoubleRainbow;
  }
  if (sa == Special::Rainbow || sb == Special::Rainbow) {
    const Special other = sa == Special::Rainbow ? sb : sa;
    return is_line(other) ? Combo::RainbowLine : Combo::RainbowBomb;
  }
  if (is_line(sa) && is_line(sb)) return Combo::Cross;
  if (sa == Special::Bomb && sb == Special::Bomb) return Combo::MegaBomb;
  return Combo::BigCross;  // The only pair left: one Line and one Bomb.
}

int SpecialBonus(Special special) {
  switch (special) {
    case Special::None:
      return 0;
    case Special::LineH:
    case Special::LineV:
      return 50;
    case Special::Bomb:
      return 100;
    case Special::Rainbow:
      return 250;
  }
  return 0;
}

// A delegating constructor: this one hands its work to the
// `(width, height, seed)` constructor. `std::random_device{}()` makes a
// temporary random_device and calls it once to read a seed from the
// operating system's entropy source.
Board::Board() : Board(kDefaultSize, kDefaultSize, std::random_device{}()) {}

Board::Board(int width, int height, std::uint32_t seed) {
  Reset(width, height, seed);
}

void Board::Reset(int width, int height, std::uint32_t seed) {
  width_ = std::clamp(width, kMinSize, kMaxSize);
  height_ = std::clamp(height, kMinSize, kMaxSize);
  rng_.seed(seed);
  pending_swap_.reset();
  cascade_ = 0;
  Fill();
}

void Board::SetLayout(const std::vector<std::string>& rows) {
  const int row_count = static_cast<int>(rows.size());
  const int column_count = rows.empty() ? 0 : static_cast<int>(rows[0].size());
  width_ = std::clamp(column_count, kMinSize, kMaxSize);
  height_ = std::clamp(row_count, kMinSize, kMaxSize);
  // `assign` resizes the vector and overwrites every element, so no gem
  // from the old board survives.
  cells_.assign(static_cast<std::size_t>(width_ * height_), std::nullopt);
  for (int y = 0; y < std::min(height_, row_count); ++y) {
    const std::string& row = rows[static_cast<std::size_t>(y)];
    for (int x = 0; x < std::min(width_, static_cast<int>(row.size())); ++x) {
      if (const auto color =
              ColorFromLetter(row[static_cast<std::size_t>(x)])) {
        cells_[static_cast<std::size_t>(Index({x, y}))] = Gem{*color};
      }
    }
  }
  pending_swap_.reset();
  cascade_ = 0;
}

void Board::SetGem(GridPoint at, std::optional<Gem> gem) {
  if (!InBounds(at)) return;
  cells_[static_cast<std::size_t>(Index(at))] = gem;
}

bool Board::IsLegalSwap(GridPoint a, GridPoint b) const {
  if (!InBounds(a) || !InBounds(b)) return false;
  // Neighbours differ by exactly 1 in x or in y, never both.
  if (std::abs(a.x - b.x) + std::abs(a.y - b.y) != 1) return false;
  const std::optional<Gem>& gem_a = At(a);
  const std::optional<Gem>& gem_b = At(b);
  if (!gem_a || !gem_b) return false;
  if (IsFreeSwap(gem_a, gem_b)) return true;
  // This method is `const`, so it cannot change `cells_`. It copies the
  // cells into a scratch vector and tries the swap there instead.
  Cells scratch = cells_;
  return SwapMakesMatch(scratch, width_, height_, a, b);
}

bool Board::ApplySwap(GridPoint a, GridPoint b) {
  if (!IsLegalSwap(a, b)) return false;
  std::swap(cells_[static_cast<std::size_t>(Index(a))],
            cells_[static_cast<std::size_t>(Index(b))]);
  pending_swap_ = Swap{a, b};
  cascade_ = 0;
  return true;
}

std::optional<ClearStep> Board::ClearMatches() {
  const auto cell_count = static_cast<std::size_t>(width_ * height_);
  // Per-cell flags: `marked` = will be cleared this step, `detonated` = its
  // special has already gone off (so it never goes off twice).
  std::vector<std::uint8_t> marked(cell_count, 0);
  std::vector<std::uint8_t> detonated(cell_count, 0);
  // Specials waiting to go off. A blast can catch more specials, which are
  // added to the end, so one blast can set off a whole chain.
  std::vector<GridPoint> queue;
  bool any_marked = false;

  ClearStep step;

  // A lambda that captures every local by reference (`[&]`), so it can
  // update `marked`, `queue`, and `any_marked` directly. That is safe here
  // because the lambda is only used inside this function, while those
  // locals are still alive.
  auto mark = [&](GridPoint p) {
    const auto i = static_cast<std::size_t>(Index(p));
    if (marked[i] != 0 || !cells_[i]) return;
    marked[i] = 1;
    any_marked = true;
    if (cells_[i]->special != Special::None && detonated[i] == 0) {
      queue.push_back(p);
    }
  };

  // Marks every matchable gem of `color` that is not already marked.
  auto mark_color = [&](GemColor color) {
    for (int y = 0; y < height_; ++y) {
      for (int x = 0; x < width_; ++x) {
        const std::optional<Gem>& cell = At({x, y});
        if (Matchable(cell) && cell->color == color) mark({x, y});
      }
    }
  };

  // Blast shapes, shared by combos (step 1) and ordinary blasts (step 3).
  auto mark_row = [&](int row) {
    if (row < 0 || row >= height_) return;
    for (int x = 0; x < width_; ++x) mark({x, row});
  };
  auto mark_column = [&](int column) {
    if (column < 0 || column >= width_) return;
    for (int y = 0; y < height_; ++y) mark({column, y});
  };
  // Marks the square of side `2 * reach + 1` centred on `centre`.
  auto mark_square = [&](GridPoint centre, int reach) {
    for (int dy = -reach; dy <= reach; ++dy) {
      for (int dx = -reach; dx <= reach; ++dx) {
        const GridPoint p{centre.x + dx, centre.y + dy};
        if (InBounds(p)) mark(p);
      }
    }
  };
  // Clears a special as part of a combo. It is flagged as detonated
  // *before* it is marked, so `mark()` does not queue it to go off a second
  // time on its own in step 3.
  auto use_up = [&](GridPoint p) {
    detonated[static_cast<std::size_t>(Index(p))] = 1;
    mark(p);
  };

  // 1. Swaps that need no match: a Rainbow with a plain gem clears every
  //    gem of that gem's colour, and two specials set off a combo.
  if (pending_swap_) {
    const Swap swap = *pending_swap_;
    // Copies, not references: the Rainbow combos below rewrite cells, and
    // these should keep describing the two gems as they were swapped.
    const std::optional<Gem> gem_a = At(swap.a);
    const std::optional<Gem> gem_b = At(swap.b);
    const bool rainbow_a = gem_a && gem_a->special == Special::Rainbow;
    const bool rainbow_b = gem_b && gem_b->special == Special::Rainbow;
    const Combo combo =
        gem_a && gem_b ? ComboFor(*gem_a, *gem_b) : Combo::None;
    step.combo = combo;
    // Combos centre on `b`, the cell the player's gem moved into.
    const GridPoint centre = swap.b;
    // For the Rainbow swaps: which cell holds the Rainbow and which the
    // other gem. With no Rainbow in the swap these are unused.
    const GridPoint rainbow = rainbow_a ? swap.a : swap.b;
    const GridPoint other = rainbow_a ? swap.b : swap.a;

    switch (combo) {
      case Combo::None:
        if (rainbow_a || rainbow_b) {
          use_up(rainbow);
          step.blasts.push_back({rainbow, Special::Rainbow});
          mark(other);
          mark_color(At(other)->color);
        }
        break;
      case Combo::DoubleRainbow:
        for (const GridPoint p : {swap.a, swap.b}) {
          use_up(p);
          step.blasts.push_back({p, Special::Rainbow});
        }
        for (int y = 0; y < height_; ++y) mark_row(y);
        break;
      case Combo::RainbowLine:
      case Combo::RainbowBomb: {
        // Turn every plain gem of the other special's colour into that kind
        // of special, then mark them all. `mark()` queues each one, so
        // step 3 sets them all off. Gems that are already specials keep
        // their own power. Line gems alternate between rows and columns in
        // a checkerboard, so the storm goes both ways.
        const GemColor color = At(other)->color;
        const bool lines = combo == Combo::RainbowLine;
        for (int y = 0; y < height_; ++y) {
          for (int x = 0; x < width_; ++x) {
            std::optional<Gem>& cell =
                cells_[static_cast<std::size_t>(Index({x, y}))];
            if (!cell || cell->special != Special::None ||
                cell->color != color) {
              continue;
            }
            if (!lines) {
              cell->special = Special::Bomb;
            } else if ((x + y) % 2 == 0) {
              cell->special = Special::LineH;
            } else {
              cell->special = Special::LineV;
            }
          }
        }
        use_up(rainbow);
        step.blasts.push_back({rainbow, Special::Rainbow});
        mark(other);
        mark_color(color);
        break;
      }
      case Combo::Cross:
        use_up(swap.a);
        use_up(swap.b);
        mark_row(centre.y);
        mark_column(centre.x);
        step.blasts.push_back({centre, Special::LineH});
        step.blasts.push_back({centre, Special::LineV});
        break;
      case Combo::BigCross:
        use_up(swap.a);
        use_up(swap.b);
        for (int d = -1; d <= 1; ++d) {
          // One beam per row and column cleared. Each beam starts in line
          // with the centre, so they all fan out from the swap.
          if (centre.y + d >= 0 && centre.y + d < height_) {
            mark_row(centre.y + d);
            step.blasts.push_back({{centre.x, centre.y + d}, Special::LineH});
          }
          if (centre.x + d >= 0 && centre.x + d < width_) {
            mark_column(centre.x + d);
            step.blasts.push_back({{centre.x + d, centre.y}, Special::LineV});
          }
        }
        break;
      case Combo::MegaBomb:
        use_up(swap.a);
        use_up(swap.b);
        mark_square(centre, 2);
        step.blasts.push_back({centre, Special::Bomb, 2});
        break;
    }
  }

  // 2. Ordinary matches. Big ones earn a special gem, placed on the gem the
  //    player moved if it is part of the match.
  for (const MatchGroup& group : FindGroups(cells_, width_, height_)) {
    for (const GridPoint p : group.cells) mark(p);
    const Special special = SpecialFor(group);
    if (special == Special::None) continue;
    GridPoint spawn = group.longest.CellAt(group.longest.length / 2);
    if (group.crossed) spawn = group.cross_cell;
    if (pending_swap_) {
      if (Contains(group.cells, pending_swap_->a)) spawn = pending_swap_->a;
      if (Contains(group.cells, pending_swap_->b)) spawn = pending_swap_->b;
    }
    step.created.push_back({spawn, Gem{group.color, special}});
  }

  pending_swap_.reset();
  if (!any_marked) return std::nullopt;

  // 3. Set off every special that got caught, and every special those
  //    blasts catch in turn. This is an index loop rather than a range-based
  //    `for` on purpose: `mark()` can `push_back` onto `queue` while we walk
  //    it, and a push_back that grows the vector invalidates the iterators a
  //    range-based `for` uses under the hood. Indices stay valid.
  for (std::size_t q = 0; q < queue.size(); ++q) {
    const GridPoint p = queue[q];
    const auto i = static_cast<std::size_t>(Index(p));
    if (detonated[i] != 0) continue;
    detonated[i] = 1;
    const Special special = cells_[i]->special;
    step.blasts.push_back({p, special});
    switch (special) {
      case Special::None:
        break;
      case Special::LineH:
        mark_row(p.y);
        break;
      case Special::LineV:
        mark_column(p.x);
        break;
      case Special::Bomb:
        mark_square(p, 1);
        break;
      case Special::Rainbow: {
        // A Rainbow caught in a blast picks the most common colour that is
        // still on the board. The braces give `counts` its own scope, which
        // C++ requires for a variable declared inside one `case`.
        std::array<int, kGemColorCount> counts{};
        for (std::size_t c = 0; c < cell_count; ++c) {
          if (marked[c] == 0 && Matchable(cells_[c])) {
            ++counts[static_cast<std::size_t>(cells_[c]->color)];
          }
        }
        // `std::ranges::max_element` returns an iterator to the largest
        // count; subtracting `begin()` turns it into an index.
        const auto best = std::ranges::max_element(counts);
        if (*best > 0) {
          mark_color(static_cast<GemColor>(best - counts.begin()));
        }
        break;
      }
    }
  }

  // 4. Remove every marked gem, then put the new specials in place.
  for (std::size_t i = 0; i < cell_count; ++i) {
    if (marked[i] == 0) continue;
    const GridPoint p{static_cast<int>(i) % width_,
                      static_cast<int>(i) / width_};
    step.cleared.push_back({p, *cells_[i]});
    cells_[i].reset();
  }
  int bonus = 0;
  for (const PlacedGem& created : step.created) {
    cells_[static_cast<std::size_t>(Index(created.at))] = created.gem;
    bonus += SpecialBonus(created.gem.special);
  }

  ++cascade_;
  step.cascade = cascade_;
  step.points =
      ScoreFor(static_cast<int>(step.cleared.size()), cascade_) + bonus;
  return step;
}

std::vector<FallMove> Board::Collapse() {
  std::vector<FallMove> moves;
  for (int x = 0; x < width_; ++x) {
    // Walk each column from the bottom up. `write` is the lowest row that
    // still needs a gem; every gem found above it slides down to it.
    int write = height_ - 1;
    for (int y = height_ - 1; y >= 0; --y) {
      // A reference into `cells_` is safe to keep here: the vector never
      // changes size in this loop, so its buffer never moves.
      std::optional<Gem>& cell =
          cells_[static_cast<std::size_t>(Index({x, y}))];
      if (!cell) continue;
      if (y != write) {
        const Gem gem = *cell;
        cells_[static_cast<std::size_t>(Index({x, write}))] = gem;
        cell.reset();
        moves.push_back({{x, write}, static_cast<float>(y), gem, false});
      }
      --write;
    }
    // Rows `0..write` are now empty. New gems start stacked above the top
    // edge, in the same order they will land, so they fall in as a column.
    const int holes = write + 1;
    for (int y = write; y >= 0; --y) {
      const Gem gem{RandomColor()};
      cells_[static_cast<std::size_t>(Index({x, y}))] = gem;
      moves.push_back({{x, y}, static_cast<float>(y - holes), gem, true});
    }
  }
  return moves;
}

CascadeResult Board::ResolveAll() {
  CascadeResult result;
  // `while (auto step = ...)` declares `step` and tests it in one go. An
  // optional converts to `true` when it holds a value, so the loop stops
  // when `ClearMatches()` returns `std::nullopt`.
  while (auto step = ClearMatches()) {
    result.points += step->points;
    result.cleared += static_cast<int>(step->cleared.size());
    ++result.steps;
    Collapse();
  }
  return result;
}

void Board::Shuffle() {
  std::vector<Gem> gems;
  for (const std::optional<Gem>& cell : cells_) {
    if (cell) gems.push_back(*cell);
  }
  // Which cells hold a gem. Shuffling keeps holes where they are.
  std::vector<std::uint8_t> occupied(cells_.size(), 0);
  for (std::size_t i = 0; i < cells_.size(); ++i) {
    occupied[i] = cells_[i].has_value() ? 1 : 0;
  }

  // Shuffling at random and hoping is not enough: with few colours, almost
  // every random arrangement has a match somewhere. Instead, deal the gems
  // out one cell at a time in reading order, and for each cell take the
  // first gem left in the (shuffled) pile that does not finish a match with
  // the two gems to its left or the two above it.
  constexpr int kAttempts = 100;
  for (int attempt = 0; attempt < kAttempts; ++attempt) {
    // `std::ranges::shuffle` needs a random engine; passing `rng_` keeps
    // shuffles repeatable for a given seed.
    std::ranges::shuffle(gems, rng_);
    cells_.assign(cells_.size(), std::nullopt);
    std::size_t next = 0;  // gems[0..next) are already dealt.
    bool stuck = false;
    for (int y = 0; y < height_ && !stuck; ++y) {
      for (int x = 0; x < width_ && !stuck; ++x) {
        const auto index = static_cast<std::size_t>(Index({x, y}));
        if (occupied[index] == 0) continue;
        // Finds the first gem in the pile that is safe to put here.
        std::size_t pick = next;
        for (; pick < gems.size(); ++pick) {
          cells_[index] = gems[pick];
          if (!MatchAt(cells_, width_, height_, {x, y})) break;
        }
        if (pick == gems.size()) {
          stuck = true;
          break;
        }
        // Move the chosen gem to the front of the undealt part of the pile.
        std::swap(gems[next], gems[pick]);
        cells_[index] = gems[next];
        ++next;
      }
    }
    if (!stuck && HasLegalMove()) {
      pending_swap_.reset();
      return;
    }
  }
  Fill();
}

bool Board::HasMatches() const {
  return !FindRuns(cells_, width_, height_).empty();
}

bool Board::HasLegalMove() const { return FindHint().has_value(); }

std::optional<Swap> Board::FindHint() const {
  // One scratch copy for the whole search; `SwapMakesMatch` swaps and swaps
  // back inside it.
  Cells scratch = cells_;
  for (int y = 0; y < height_; ++y) {
    for (int x = 0; x < width_; ++x) {
      const GridPoint a{x, y};
      // Only right and down: left and up are the same swaps seen from the
      // other cell.
      for (const GridPoint b : {GridPoint{x + 1, y}, GridPoint{x, y + 1}}) {
        if (!InBounds(b) || !At(a) || !At(b)) continue;
        if (IsFreeSwap(At(a), At(b)) ||
            SwapMakesMatch(scratch, width_, height_, a, b)) {
          return Swap{a, b};
        }
      }
    }
  }
  return std::nullopt;
}

bool Board::InBounds(GridPoint at) const {
  return at.x >= 0 && at.x < width_ && at.y >= 0 && at.y < height_;
}

const std::optional<Gem>& Board::At(GridPoint at) const {
  if (!InBounds(at)) return kEmptyCell;
  return cells_[static_cast<std::size_t>(Index(at))];
}

void Board::Fill() {
  constexpr int kAttempts = 100;
  for (int attempt = 0; attempt < kAttempts; ++attempt) {
    cells_.assign(static_cast<std::size_t>(width_ * height_), std::nullopt);
    for (int y = 0; y < height_; ++y) {
      for (int x = 0; x < width_; ++x) {
        // Ban a colour if the two gems to the left, or the two above, are
        // both that colour: placing it here would make a match of 3.
        std::array<bool, kGemColorCount> banned{};
        if (x >= 2 && SameColor(At({x - 1, y}), At({x - 2, y}))) {
          banned[static_cast<std::size_t>(At({x - 1, y})->color)] = true;
        }
        if (y >= 2 && SameColor(At({x, y - 1}), At({x, y - 2}))) {
          banned[static_cast<std::size_t>(At({x, y - 1})->color)] = true;
        }
        std::vector<GemColor> allowed;
        for (int c = 0; c < kGemColorCount; ++c) {
          if (!banned[static_cast<std::size_t>(c)]) {
            allowed.push_back(static_cast<GemColor>(c));
          }
        }
        // At most two colours are banned, so at least four are left.
        std::uniform_int_distribution<std::size_t> pick(0, allowed.size() - 1);
        cells_[static_cast<std::size_t>(Index({x, y}))] =
            Gem{allowed[pick(rng_)]};
      }
    }
    if (HasLegalMove()) return;
  }
}

GemColor Board::RandomColor() {
  // `uniform_int_distribution` maps the engine's raw 32-bit output onto
  // `0..5` without the bias that `rng_() % 6` would have.
  std::uniform_int_distribution<int> pick(0, kGemColorCount - 1);
  return static_cast<GemColor>(pick(rng_));
}

}  // namespace match3
