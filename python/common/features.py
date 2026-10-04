"""Hand-crafted Tetris features for rule-based baselines.

This project uses a selection of hand-crafted board features for a lightweight
BCTS-inspired baseline, not a full reproduction of a published controller.
The features are useful in two ways:

1. As an evaluation reference under the same rules, opponents and budget.
   A lower policy score motivates investigation of the objective, data and
   implementation; it does not by itself prove a training bug.
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

import math
from numbers import Integral

import numpy as np

from . import BOARD_COLS, BOARD_ROWS


def _occupied(board):
    raw = np.asarray(board)
    if raw.shape != (BOARD_ROWS, BOARD_COLS):
        raise ValueError("board must match the shared row/column schema")
    if raw.dtype.kind not in "biuf" or not np.isfinite(raw).all():
        raise ValueError("board cells must be finite real integer values")
    if (raw < 0).any() or (raw.dtype.kind == "f" and (raw != np.floor(raw)).any()):
        raise ValueError("board cells must be nonnegative integers")
    # Raw native IDs and binary observations use the same locked-cell predicate.
    return (raw > 0) & (raw != 8)


def _heights(values):
    raw = np.asarray(values)
    if raw.shape != (BOARD_COLS,) or raw.dtype.kind not in "biuf":
        raise ValueError("heights must match the column schema")
    if (not np.isfinite(raw).all() or (raw < 0).any() or (raw > BOARD_ROWS).any()
            or (raw.dtype.kind == "f" and (raw != np.floor(raw)).any())):
        raise ValueError("heights must be integers within the board")
    return raw.astype(np.int64)


def column_heights(board: np.ndarray) -> np.ndarray:
    """For each column, the row index of the topmost occupied cell mapped to a
    height: a topmost cell at row 0 has height BOARD_ROWS, while an empty
    column has height 0.
    """
    occupied = _occupied(board)
    # 열마다 제일 위에 있는 블록의 행 번호. 빈 열은 BOARD_ROWS로 둔다.
    # 이 값 하나로 높이, 구멍, 요철을 전부 계산할 수 있다.
    first_filled = np.where(
        occupied.any(axis=0),
        occupied.argmax(axis=0),
        BOARD_ROWS,
    )
    return BOARD_ROWS - first_filled


def aggregate_height(heights: np.ndarray) -> int:
    return int(_heights(heights).sum())


def bumpiness(heights: np.ndarray) -> int:
    return int(np.abs(np.diff(_heights(heights))).sum())


def count_holes(board: np.ndarray) -> int:
    """A hole is an empty cell with at least one filled cell somewhere above it
    in the same column. We count every such cell, not just the topmost per
    column.
    """
    occupied = _occupied(board)
    holes = 0
    for col in range(BOARD_COLS):
        col_view = occupied[:, col]
        if not col_view.any():
            continue
        top = int(np.argmax(col_view))
        holes += int((~col_view[top:]).sum())
    return holes


def max_height(heights: np.ndarray) -> int:
    return int(_heights(heights).max())


def well_sum(heights: np.ndarray) -> int:
    """Sum of well depths. A well at column ``i`` is ``max(0, min(left,right) - h_i)``,
    where ``left`` and ``right`` use ``BOARD_ROWS`` for the borders.
    """
    heights = _heights(heights)
    total = 0
    for i in range(BOARD_COLS):
        left = heights[i - 1] if i - 1 >= 0 else BOARD_ROWS
        right = heights[i + 1] if i + 1 < BOARD_COLS else BOARD_ROWS
        depth = min(left, right) - heights[i]
        if depth > 0:
            # 깊이 d인 well은 d*(d+1)/2점으로 센다.
            # 표면 높이로 정의한 근사 특징이며 실제 도형의 도달 경로는 검사하지 않는다.
            total += depth * (depth + 1) // 2
    return int(total)


def all_features(board: np.ndarray, rows_cleared: int) -> dict[str, int]:
    if (isinstance(rows_cleared, (bool, np.bool_))
            or not isinstance(rows_cleared, Integral) or not 0 <= rows_cleared <= BOARD_ROWS):
        raise ValueError("rows_cleared must be an integer within the board height")
    h = column_heights(board)
    return {
        "aggregate_height": aggregate_height(h),
        "bumpiness": bumpiness(h),
        "holes": count_holes(board),
        "max_height": max_height(h),
        "rows_cleared": int(rows_cleared),
        "wells": well_sum(h),
    }


# 이 저장소의 선형 보드 평가 계수. 전체 평가값이 클수록 선호한다.
# 일부 특징은 계수가 0이며, 계수의 출처나 성능은 이 선언만으로 보장되지 않는다.
BCTS_WEIGHTS = {
    "aggregate_height": -0.510066,
    "bumpiness":        -0.184483,
    "holes":            -0.35663,
    "max_height":        0.0,      # disabled by this configuration; distinct from the height sum
    "rows_cleared":      0.760666,
    "wells":            -0.1,
}


def bcts_score(board: np.ndarray, rows_cleared: int) -> float:
    feats = all_features(board, rows_cleared)
    try:
        score = float(sum(BCTS_WEIGHTS[k] * v for k, v in feats.items()))
    except (TypeError, ValueError, OverflowError) as exc:
        raise ValueError("feature weights must produce a finite score") from exc
    if not math.isfinite(score):
        raise ValueError("feature score is not finite")
    return score
