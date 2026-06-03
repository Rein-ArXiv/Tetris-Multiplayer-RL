# Thin CMake wrapper.
#
# The project build is defined in CMakeLists.txt. This Makefile exists only so
# Linux/macOS users can type `make` from the repository root without falling
# into the old raylib sample build path.

.PHONY: all configure build game test relay meta py python-sync python-test python-test-core clean release-linux

BUILD_DIR ?= build
CONFIG ?= Release
CMAKE ?= cmake
CMAKE_ARGS ?=

all: build

configure:
	$(CMAKE) -S . -B $(BUILD_DIR) $(CMAKE_ARGS)

build: configure
	$(CMAKE) --build $(BUILD_DIR) --config $(CONFIG)

game: configure
	$(CMAKE) --build $(BUILD_DIR) --config $(CONFIG) --target tetris

test: configure
	$(CMAKE) --build $(BUILD_DIR) --config $(CONFIG) --target sim_hash_dump
	$(BUILD_DIR)/sim_hash_dump

relay: CMAKE_ARGS += -DTETRIS_BUILD_GAME=OFF -DTETRIS_BUILD_RELAY=ON
relay: configure
	$(CMAKE) --build $(BUILD_DIR) --config $(CONFIG) --target tetris_relay

meta: CMAKE_ARGS += -DTETRIS_BUILD_GAME=OFF -DTETRIS_BUILD_META=ON
meta: configure
	$(CMAKE) --build $(BUILD_DIR) --config $(CONFIG) --target tetris_meta

py: CMAKE_ARGS += -DTETRIS_BUILD_GAME=OFF -DTETRIS_BUILD_PY=ON
py: configure
	$(CMAKE) --build $(BUILD_DIR) --config $(CONFIG) --target tetris_py

python-sync:
	uv sync --dev

python-test:
	uv run python -m pytest python/tests

python-test-core:
	uv run python -m pytest \
		python/tests/test_framing_parity.py \
		python/tests/test_checkpoint_roundtrip.py \
		-q

clean:
	$(CMAKE) -E rm -rf $(BUILD_DIR)

release-linux:
	./scripts/release_linux.sh
