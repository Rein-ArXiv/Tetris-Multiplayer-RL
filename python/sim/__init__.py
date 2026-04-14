"""Headless Tetris simulation — Python wrapper around the C++ ``tetris_py`` module.

This package re-exports the native pybind11 module ``tetris_py`` so that the rest
of the codebase (training in Colab, netbot inference on Windows, tests) can use a
single stable import path::

    from sim import SimGame, Placement

Build instructions
------------------
The native module is **not** auto-built. You must build it yourself before
importing::

    # Linux / Colab (no raylib needed)
    cmake -S . -B build -DTETRIS_BUILD_GAME=OFF -DTETRIS_BUILD_PY=ON
    cmake --build build -j --target tetris_py
    cp build/tetris_py*.so python/sim/

    # Windows (MSVC or w64devkit + python headers)
    cmake -S . -B build -DTETRIS_BUILD_PY=ON
    cmake --build build --config Release --target tetris_py
    copy build\\Release\\tetris_py*.pyd python\\sim\\

The shared object / .pyd lives next to this ``__init__.py`` so the import works
without any extra ``sys.path`` mangling beyond adding ``python/`` itself.
"""

from __future__ import annotations

import sys
from pathlib import Path

# Make sure ``python/sim/`` is on sys.path so the freshly-copied native module is
# discoverable even if the user only put ``python/`` on sys.path. This is a
# defensive belt-and-braces measure — once the package is pip-installed it
# becomes a no-op.
_HERE = Path(__file__).resolve().parent
if str(_HERE) not in sys.path:
    sys.path.insert(0, str(_HERE))

try:
    from tetris_py import SimGame, Placement, SimBlock  # type: ignore
except ImportError as exc:  # pragma: no cover - environment failure path
    raise ImportError(
        "Could not import the native 'tetris_py' module.\n\n"
        "Build it first:\n"
        "  cmake -S . -B build -DTETRIS_BUILD_PY=ON\n"
        "  cmake --build build -j --target tetris_py\n\n"
        "Then drop the resulting tetris_py.*.{so,pyd} into python/sim/.\n"
        "See python/sim/__init__.py for the full instructions."
    ) from exc


__all__ = ["SimGame", "Placement", "SimBlock"]
