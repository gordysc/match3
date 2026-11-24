# Match-3

This is a match-3 puzzle game in **C++20** with [raylib](https://www.raylib.com/). A pure C++20 core library (`match3_core`) holds the rules. The game draws every gem from shapes and synthesizes every sound in code, so it has no asset files.

## Highlights

- **Swap and match**: Swap two neighbouring gems to make a line of 3 or more of the same colour. A swap that makes no match bounces back.
- **Special gems**:
  - Match 4 in a line to make a **Line gem**. When it clears, it clears its whole row (from a horizontal match) or column (from a vertical match).
  - Match an **L or T shape** to make a **Bomb**. When it clears, it clears the 3x3 square around it.
  - Match 5 in a line to make a **Rainbow**. Swap it with any gem to clear every gem of that colour. Swap two Rainbows to clear the whole board.
  - A blast also sets off the special gems that it hits, so one blast can start a chain.
- **Cascades**: Gems fall into the holes and new gems drop in from the top. Each new match in the same move multiplies its points: x2, x3, and so on.
- **Three modes**: **Moves** (30 swaps), **Blitz** (90 seconds, the clock starts on your first swap), and **Zen** (no limit).
- **Never stuck**: A new board has no matches and always has a legal swap. When no swap is left, the board reshuffles. After 7 idle seconds, a hint shows a legal swap.
- **Colour-blind friendly**: Each colour has its own shape: circle, square, diamond, triangle, hexagon, and pentagon.
- **Effects**: Sparks, swap and fall animations, beams, shockwaves, screen shake, and cascade banners.
- **Procedural audio**: The game synthesizes all of its sounds at startup. On each cascade step the clear chime plays one note higher on a pentatonic scale.
- **Records**: The game saves the best score, the games played, and the longest cascade for each mode to `~/.match3.cfg`.

## Build in CLion

1. Open this folder in CLion.
2. Make sure that the computer has an internet connection. On the first load, CMake downloads raylib 5.5 with `FetchContent`.
3. Select the `match3` run configuration.
4. Push Run.

To run the core test suite, select the `match3_tests` run configuration and push Run.

## Build and Test from the Command Line

```bash
cmake -S . -B build
cmake --build build --parallel
ctest --test-dir build --output-on-failure
./build/match3
```

Or use the Makefile wrapper: `make run` (build and launch), `make test`, `make clean`.

## Controls

| Action | Mouse | Keyboard |
| --- | --- | --- |
| Swap two gems | Drag a gem onto a neighbour, or click one gem and then its neighbour | Move the cursor, `Space` to pick up, then an arrow key |
| Move the cursor | — | Arrow keys or `WASD` |
| Put a gem back down | Click it again | `Space` |
| Show a hint | Click **HINT** | `H` |
| Pause | — | `P` |
| Sound on or off | Click **SOUND** | `M` |
| Start a new game | Click **NEW GAME** | `R` or `F2` |
| Change the mode | Click a tab | `1`, `2`, or `3` |
| Quit | Close the window | `Esc` |

## Points

- Each cleared gem gives 10 points. The game multiplies these points by the cascade step. The swap's own clear is step 1.
- A new special gem gives a bonus: Line +50, Bomb +100, Rainbow +250.

## Project Layout

- `src/gem.h`, `src/board.*`, `src/mode.*`, `src/storage.*`: The pure C++20 core (`match3_core`). It has no raylib dependency. The Board resolves a move in steps (`ClearMatches()`, then `Collapse()`, again and again), so the game can animate between the steps.
- `src/layout.*`, `src/theme.h`, `src/effects.*`, `src/audio.*`, `src/renderer.*`, `src/game.*`, `src/main.cpp`: The raylib screen layout, the colours, the animations, the synthesized sound, the draw code, and the game loop with its phase state machine.
- `tests/match3_tests.cpp`: The headless CTest suite. It tests the board fill, illegal swaps, matches of 3, 4, and 5, L shapes, Line, Bomb, and Rainbow blasts, chain reactions, gravity and refill, cascade points, the shuffle of a dead board, 1,000 moves of self-play with hints, the mode rules, and the save file.
