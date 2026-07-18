# `sim` — Python wrapper around the C++ headless Tetris simulator

`sim` re-exports the native `tetris_py` pybind11 module so the rest of the
codebase can write `from sim import SimGame` regardless of whether the build
artifact is a platform/version-specific `.so` (Linux/macOS) or `.pyd`
(Windows).

The native module is **not** auto-built. Build it once per platform.

## Linux / Colab

The Colab notebook (`python/train/train_model_zoo_colab.ipynb`) does this in
its setup cells. The manual equivalent is:

```bash
sudo apt-get install -y cmake g++ python3-dev
uv sync --dev
PYBIND11_DIR=$(uv run python -m pybind11 --cmakedir)

rm -rf build
rm -f python/sim/tetris_py*.so
cmake -S . -B build \
    -DCMAKE_BUILD_TYPE=Release \
    -DTETRIS_BUILD_GAME=OFF \
    -DTETRIS_BUILD_PY=ON \
    -Dpybind11_DIR=$PYBIND11_DIR
cmake --build build -j --target tetris_py
cp build/tetris_py*.so python/sim/
```

`-DTETRIS_BUILD_GAME=OFF` skips the game executable, so Colab only builds the
headless sim module.

## Windows (MSVC or w64devkit)

```cmd
uv sync --dev
for /f "delims=" %i in ('uv run python -m pybind11 --cmakedir') do set PYBIND11_DIR=%i

cmake -S . -B build ^
    -DCMAKE_BUILD_TYPE=Release ^
    -DTETRIS_BUILD_PY=ON ^
    -Dpybind11_DIR=%PYBIND11_DIR%
cmake --build build --config Release --target tetris_py
copy build\Release\tetris_py*.pyd python\sim\
```

Note that on Windows you can leave `-DTETRIS_BUILD_GAME=ON` (the default) so
the same configure pass also builds the handmade OpenGL `tetris.exe`.

## Smoke test

```python
import sys; sys.path.insert(0, 'python')
from sim import SimGame
g = SimGame(seed=42)
print(hex(g.state_hash()))
print(g.legal_placements()[:3])
branch = g.clone()
print(branch.apply_placement(g.legal_placements()[0].col, g.legal_placements()[0].rot))
```

The printed `state_hash` value must be **identical** between Linux and Windows
builds with the same seed. If they diverge, the cross-platform determinism
gate has failed; training and in-game inference may then observe different
states and choose different placements.

Use the C++ `sim_hash_dump` test driver to compare:

```bash
build/sim_hash_dump > linux_hashes.txt
# on Windows:
build\Release\sim_hash_dump.exe > windows_hashes.txt
diff linux_hashes.txt windows_hashes.txt   # must be empty
```
