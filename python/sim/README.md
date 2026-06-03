# `sim` — Python wrapper around the C++ headless Tetris simulator

`sim` re-exports the native `tetris_py` pybind11 module so the rest of the
codebase can write `from sim import SimGame` regardless of whether the build
artifact is `tetris_py.cpython-311-x86_64-linux-gnu.so` (Colab) or
`tetris_py.cp311-win_amd64.pyd` (Windows).

The native module is **not** auto-built. Build it once per platform.

## Linux / Colab

The setup notebook (`python/train/setup_colab.ipynb`) does this for you. The
manual equivalent is:

```bash
sudo apt-get install -y cmake g++ python3-dev
uv sync --dev
PYBIND11_DIR=$(uv run python -m pybind11 --cmakedir)

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
```

The printed `state_hash` value must be **identical** between Linux and Windows
builds with the same seed. If they diverge, the cross-platform determinism
gate has failed and any RL policy trained on one platform will desync when
deployed on the other.

Use the C++ `sim_hash_dump` test driver to compare:

```bash
build/sim_hash_dump > linux_hashes.txt
# on Windows:
build\Release\sim_hash_dump.exe > windows_hashes.txt
diff linux_hashes.txt windows_hashes.txt   # must be empty
```
