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
Pieces with fewer than 4 distinct rotations (O, and the 2-state pieces) still
enumerate all 4, so the mask keeps several *duplicate* actions that differ in
index but land identically. This is deliberate: the action index must mean the
same thing in C++ and Python, which rules out compacting the space per piece.
"""

from __future__ import annotations

# action space 상수를 여기 모아 둔다. models.py, obs.py, action_mask.py, env.py가
# 전부 이 값을 참조하는데, 어느 한 모듈에 두면 서로 import하다 순환이 생긴다.
NUM_COLS = 10
NUM_ROTATIONS = 4
NUM_PLACEMENTS = NUM_COLS * NUM_ROTATIONS  # 40

# 블록 종류 수. ID는 0이 아니라 1부터 시작한다(src/sim_blocks.h 기준).
NUM_PIECE_TYPES = 7

# 보드 크기. SimGrid::kRows / kCols와 어긋나면 관측 텐서 shape이 안 맞는다.
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
