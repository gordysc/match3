# Convenience wrapper around CMake. CMake stays the real build system;
# these targets just save typing the configure/build/run commands.
#
#   make          show the available targets (same as `make help`)
#   make build    build the game (Release)
#   make run      build, then launch the game
#   make test     build and run the core unit tests
#   make debug    build and launch a Debug build
#   make clean    delete all build output

# Use cmake from PATH if there is one, otherwise fall back to the copy
# bundled with CLion. Override with `make CMAKE=/path/to/cmake`.
CMAKE ?= $(or $(shell command -v cmake 2>/dev/null),\
              $(firstword $(wildcard /Applications/CLion.app/Contents/bin/cmake/mac/*/bin/cmake)))
CTEST ?= $(dir $(CMAKE))ctest

BUILD_TYPE ?= Release
BUILD_DIR  ?= build
DEBUG_DIR  ?= build-debug

.DEFAULT_GOAL := help
.PHONY: build run test debug clean help check-cmake

check-cmake:
	@if [ -z "$(CMAKE)" ] || ! [ -x "$(CMAKE)" ]; then \
		echo "error: cmake not found. Install it (brew install cmake) or pass CMAKE=/path/to/cmake." >&2; \
		exit 1; \
	fi

# Configure on every build so BUILD_TYPE is always applied, even to a build
# folder CLion or a manual cmake run already set up. With an existing cache
# this takes under a second. -Wno-author hides raylib's OpenGL deprecation notice.
build: check-cmake
	"$(CMAKE)" -S . -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=$(BUILD_TYPE) -Wno-author
	"$(CMAKE)" --build $(BUILD_DIR) --parallel

run: build
	./$(BUILD_DIR)/match3

test: build
	"$(CTEST)" --test-dir $(BUILD_DIR) --output-on-failure

debug: check-cmake
	"$(CMAKE)" -S . -B $(DEBUG_DIR) -DCMAKE_BUILD_TYPE=Debug -Wno-author
	"$(CMAKE)" --build $(DEBUG_DIR) --parallel
	./$(DEBUG_DIR)/match3

clean:
	rm -rf $(BUILD_DIR) $(DEBUG_DIR)

help:
	@echo "Match-3 — available targets:"
	@echo
	@echo "  make help    show this help (default)"
	@echo "  make build   build the game ($(BUILD_TYPE)) into $(BUILD_DIR)/"
	@echo "  make run     build and launch the game"
	@echo "  make test    build and run the unit tests"
	@echo "  make debug   build and launch a Debug build from $(DEBUG_DIR)/"
	@echo "  make clean   delete $(BUILD_DIR)/ and $(DEBUG_DIR)/"
	@echo
	@echo "Variables (pass as make VAR=value):"
	@echo "  BUILD_TYPE   build type for build/run/test (now: $(BUILD_TYPE))"
	@echo "  CMAKE        cmake binary (now: $(or $(CMAKE),<not found>))"
