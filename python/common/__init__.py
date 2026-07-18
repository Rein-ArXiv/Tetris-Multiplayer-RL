"""Shared training/inference layer for the Tetris RL bot.

This package is the **single source of truth** for everything that crosses the
Colab-training to local-inference boundary:

- ``models``      — neural network architectures with versioned ``ARCH_VERSION``
- ``obs``         — ``SimGame`` to observation tensor builder (training rollouts
  and the C++ in-game bot's ``observe()`` (bot/placement.cpp) must stay in sync)
- ``action_mask`` — placement-level legal-action masks
- ``features``    — hand-crafted Tetris features (BCTS) for rule-based baselines
- ``checkpoint``  — ``save_checkpoint`` / ``load_checkpoint`` with arch-version
  guarding so a Colab arch change cannot be silently loaded at ONNX-export time
- ``env``         — Gymnasium-compatible env so external RL frameworks (CleanRL,
  SB3, LightZero, RLlib) can plug in without bespoke glue
- ``env_versus`` — two-board garbage environment with scripted/policy opponents

The placement action space is fixed at ``COLS * ROTATIONS == 10 * 4 == 40``.
Some pieces have fewer than 4 unique rotations; the legal mask zeros those out.
"""

from __future__ import annotations

# Action-space constants — kept here so models.py, obs.py, action_mask.py and
# env.py all agree without circular imports.
NUM_COLS = 10
NUM_ROTATIONS = 4
NUM_PLACEMENTS = NUM_COLS * NUM_ROTATIONS  # 40

# Number of distinct piece IDs (1..7 — see src/sim_blocks.h).
NUM_PIECE_TYPES = 7

# Board dimensions — must match SimGrid::kRows / kCols.
BOARD_ROWS = 20
BOARD_COLS = 10

__all__ = [
    "NUM_COLS",
    "NUM_ROTATIONS",
    "NUM_PLACEMENTS",
    "NUM_PIECE_TYPES",
    "BOARD_ROWS",
    "BOARD_COLS",
]
