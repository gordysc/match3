#include <raylib.h>

#include "game.h"
#include "screen.h"

/// @brief Opens the window and audio device, runs the game loop, and cleans
/// up raylib resources in reverse initialization order.
/// @return 0 on a normal exit.
int main() {
  // Ask for 4x multisample anti-aliasing before the OpenGL context exists, so
  // gem shapes, circles, and rings have smooth edges.
  SetConfigFlags(FLAG_MSAA_4X_HINT);
  InitWindow(match3::kScreenWidth, match3::kScreenHeight, "Match-3");
  // The audio device must be open before `Game` builds its sounds. If it
  // fails (no sound card), the game still runs, just silently.
  InitAudioDevice();
  SetTargetFPS(60);

  // Explicit inner scope (RAII): `game` is destroyed at the closing brace, so
  // its destructor saves records and unloads sounds while raylib is still
  // initialized, before CloseAudioDevice() and CloseWindow() tear down the
  // underlying backends.
  {
    match3::Game game;
    // raylib's default exit key is Esc, so Esc (or the window's close
    // button) ends the loop.
    while (!WindowShouldClose()) {
      game.Update(GetFrameTime());

      BeginDrawing();
      game.Draw();
      EndDrawing();
    }
  }

  CloseAudioDevice();
  CloseWindow();
  return 0;
}
