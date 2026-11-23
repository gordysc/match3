cmake_minimum_required(VERSION 3.24)
project(match3 LANGUAGES C CXX)

# Compile as C++20, and fail rather than quietly fall back to an older
# standard if the compiler doesn't support it.
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# Write compile_commands.json into the build folder. Tools like clangd and
# clang-tidy read it to learn the exact compiler flags for each source file.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Fetch raylib at configure time (no system install needed).
include(FetchContent)
# Skip raylib's example programs and games; we only need the library itself.
# `CACHE BOOL "" FORCE` overrides raylib's own default option values.
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_GAMES OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    raylib
    GIT_REPOSITORY https://github.com/raysan5/raylib.git
    GIT_TAG 5.5
    # GIT_SHALLOW TRUE downloads only tag 5.5 instead of raylib's full commit
    # history, keeping the first configure fast.
    GIT_SHALLOW TRUE
)
# raylib's CMakeLists.txt asks for a minimum CMake version below 3.10, which
# makes newer CMake print a deprecation warning meant for raylib's own
# developers. CMAKE_POLICY_VERSION_MINIMUM (CMake 4.0+) tells CMake to treat
# that request as 3.10 instead, so the warning goes away. It is set only
# while raylib is set up, then removed so it doesn't affect anything else.
set(CMAKE_POLICY_VERSION_MINIMUM 3.10)
FetchContent_MakeAvailable(raylib)
unset(CMAKE_POLICY_VERSION_MINIMUM)

# Build raylib's own source files with compiler warnings off (-w). They are
# third-party code we don't change, so the build log only shows warnings
# from this project's code. PRIVATE applies the flag only while compiling
# raylib itself, not to targets that link against raylib.
target_compile_options(raylib PRIVATE -w)

# `match3_core` is a STATIC library (archived into libmatch3_core.a) holding
# all pure C++20 game rules with zero raylib dependency. Keeping filling,
# swapping, matching, special gems, gravity, shuffling, and persistence
# separate from graphics lets the unit tests compile quickly and run
# headlessly in CI.
add_library(match3_core STATIC
    src/board.cpp
    src/mode.cpp
    src/storage.cpp
)
# PUBLIC include directories apply both when compiling `match3_core` itself
# AND automatically propagate to every target that links against it (both
# `match3` and `match3_tests`), so `#include "board.h"` works everywhere.
target_include_directories(match3_core PUBLIC src)
# -Wall and -Wextra turn on most useful compiler warnings; -Wpedantic warns
# about non-standard C++ compiler extensions. PRIVATE keeps these flags scoped
# to compiling `match3_core`'s own .cpp files.
target_compile_options(match3_core PRIVATE -Wall -Wextra -Wpedantic)

# `match3` is the playable game executable. It pairs the pure `match3_core`
# rules library with raylib rendering, mouse/keyboard input, animations, and
# procedurally synthesized sound.
add_executable(match3
    src/audio.cpp
    src/effects.cpp
    src/game.cpp
    src/layout.cpp
    src/main.cpp
    src/renderer.cpp
)
# PRIVATE means only `match3` itself uses `match3_core` and `raylib`. PUBLIC
# would pass them on to anything linking against `match3`, and INTERFACE
# would pass them on without using them ourselves. For a final executable
# that nothing links against, PRIVATE is always the right visibility.
target_link_libraries(match3 PRIVATE match3_core raylib)
target_compile_options(match3 PRIVATE -Wall -Wextra -Wpedantic)

# macOS system frameworks raylib needs: windows (Cocoa), keyboard and
# controller input (IOKit), graphics (OpenGL), frame timing (CoreVideo), and
# audio output (CoreAudio, AudioToolbox) for the synthesized sound effects.
if (APPLE)
    target_link_libraries(match3 PRIVATE
        "-framework Cocoa"
        "-framework IOKit"
        "-framework OpenGL"
        "-framework CoreVideo"
        "-framework CoreAudio"
        "-framework AudioToolbox")
endif()

# Enable CTest (`ctest --test-dir build`). BUILD_TESTING defaults to ON and can
# be turned off with `-DBUILD_TESTING=OFF` if someone only wants the game.
include(CTest)
if (BUILD_TESTING)
    # Standalone test executable linking only `match3_core` (no raylib/window),
    # so it runs fast and headlessly on macOS and Linux CI.
    add_executable(match3_tests tests/match3_tests.cpp)
    target_link_libraries(match3_tests PRIVATE match3_core)
    target_compile_options(match3_tests PRIVATE -Wall -Wextra -Wpedantic)
    add_test(NAME match3_core_tests COMMAND match3_tests)
endif()
