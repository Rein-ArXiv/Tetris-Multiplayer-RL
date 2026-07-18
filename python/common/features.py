"""Hand-crafted Tetris features for rule-based baselines.

These are the classic BCTS (Building Controllers for Tetris) features used by
Dellacherie's algorithm and many follow-up works. They're useful in two ways:

1. As a sanity check: a linear combination of these features beats random play
   by orders of magnitude. If your trained policy can't outscore the BCTS
   baseline, training has a bug.
2. As a board evaluator inside training loops (``bcts_score`` is used by the
   CBMPI/DQN reward shaping in python/train/).

The feature set:

- ``aggregate_height``  : sum of column heights
- ``bumpiness``          : sum of |height[i] - height[i+1]| over adjacent columns
- ``holes``              : number of empty cells with at least one filled cell above
- ``max_height``         : tallest column
- ``rows_cleared``       : passed in by the caller (it depends on the action)
- ``wells``              : sum over columns of well depths

All features operate on the **post-placement** board state.
"""

from __future__ import annotations

import numpy as np

from . import BOARD_COLS, BOARD_ROWS


def column_heights(board: np.ndarray) -> np.ndarray:
    """For each column, the row index of the topmost occupied cell mapped to a
    height: a column with the topmost cell at row 0 has height 20, an empty
    column has height 0.
    """
    occupied = board > 0
    # First occupied row per column, or BOARD_ROWS if the column is empty.
    first_filled = np.where(
        occupied.any(axis=0),
        occupied.argmax(axis=0),
        BOARD_ROWS,
    )
    return BOARD_ROWS - first_filled


def aggregate_height(heights: np.ndarray) -> int:
    return int(heights.sum())


def bumpiness(heights: np.ndarray) -> int:
    return int(np.abs(np.diff(heights)).sum())


def count_holes(board: np.ndarray) -> int:
    """A hole is an empty cell with at least one filled cell directly above it
    in the same column. We count every such cell, not just the topmost per
    column.
    """
    occupied = board > 0
    holes = 0
    for col in range(BOARD_COLS):
        col_view = occupied[:, col]
        if not col_view.any():
            continue
        top = int(np.argmax(col_view))
        holes += int((~col_view[top:]).sum())
    return holes


def max_height(heights: np.ndarray) -> int:
    return int(heights.max())


def well_sum(heights: np.ndarray) -> int:
    """Sum of well depths. A well at column ``i`` is ``max(0, min(left,right) - h_i)``,
    where ``left`` and ``right`` use ``BOARD_ROWS`` for the borders.
    """
    total = 0
    for i in range(BOARD_COLS):
        left = heights[i - 1] if i - 1 >= 0 else BOARD_ROWS
        right = heights[i + 1] if i + 1 < BOARD_COLS else BOARD_ROWS
        depth = min(left, right) - heights[i]
        if depth > 0:
            # Triangular sum: a depth-d well contributes d*(d+1)/2 (deeper cells
            # are weighted more — matches the original BCTS formulation).
            total += depth * (depth + 1) // 2
    return int(total)


def all_features(board: np.ndarray, rows_cleared: int) -> dict[str, int]:
    h = column_heights(board)
    return {
        "aggregate_height": aggregate_height(h),
        "bumpiness": bumpiness(h),
        "holes": count_holes(board),
        "max_height": max_height(h),
        "rows_cleared": int(rows_cleared),
        "wells": well_sum(h),
    }


# Dellacherie's classic linear weights — known to play decent Tetris on a
# standard board. These are negative for "bad" features (we want to *minimize*
# them) and positive for cleared lines.
BCTS_WEIGHTS = {
    "aggregate_height": -0.510066,
    "bumpiness":        -0.184483,
    "holes":            -0.35663,
    "max_height":        0.0,      # subsumed by aggregate_height
    "rows_cleared":      0.760666,
    "wells":            -0.1,
}


def bcts_score(board: np.ndarray, rows_cleared: int) -> float:
    feats = all_features(board, rows_cleared)
    return float(sum(BCTS_WEIGHTS[k] * v for k, v in feats.items()))
