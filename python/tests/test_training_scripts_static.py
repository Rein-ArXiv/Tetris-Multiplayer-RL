"""Static checks for Colab training entrypoints.

These tests intentionally do not import torch or run training. They only catch
syntax errors in scripts that are expected to run in the Colab environment.
"""

from __future__ import annotations

import ast
from pathlib import Path


def test_colab_training_scripts_parse() -> None:
    root = Path(__file__).resolve().parents[1]
    for rel in [
        "train/rl_common.py",
        "train/dqn_tetris.py",
        "train/cbmpi_tetris.py",
        "train/muzero_tetris.py",
        "train/policy_gradient_tetris.py",
        "train/cem_tetris.py",
    ]:
        path = root / rel
        ast.parse(path.read_text(encoding="utf-8"), filename=str(path))
