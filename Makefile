# Thin wrapper around CMake. `make` builds, `make run` builds and launches.
BUILD_DIR  ?= build
BUILD_TYPE ?= Release

# Use cmake from PATH; otherwise fall back to the copy bundled with CLion.
CLION_CMAKE_BIN := $(firstword $(wildcard /Applications/CLion.app/Contents/bin/cmake/mac/*/bin))
CMAKE ?= $(or $(shell command -v cmake 2>/dev/null),$(CLION_CMAKE_BIN)/cmake)
CTEST ?= $(dir $(CMAKE))ctest

.PHONY: all configure build run test clean

all: build

configure: $(BUILD_DIR)/CMakeCache.txt

$(BUILD_DIR)/CMakeCache.txt:
	$(CMAKE) -S . -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=$(BUILD_TYPE)

build: configure
	$(CMAKE) --build $(BUILD_DIR) --parallel

run: build
	./$(BUILD_DIR)/match3

test: build
	$(CTEST) --test-dir $(BUILD_DIR) --output-on-failure

clean:
	rm -rf $(BUILD_DIR)
