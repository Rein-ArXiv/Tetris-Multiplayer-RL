"""Reference-behavior tests for ``netbot.input_expander.expand_placement``.

The C++ port at ``bot/placement.cpp`` is byte-for-byte identical to the Python
implementation here. This test file pins down the Python side so that a future
edit that silently changes the sequence order (e.g. "let's move before
rotating") fails loud, and the C++ port must be updated in lockstep.

If we later wire a pybind11 binding for ``bot::expand_placement``, extend this
file with a direct C++-vs-Python comparison. For now the exhaustive table below
plus the invariants documented in :func:`expand_placement`'s docstring are the
contract.
"""

from __future__ import annotations

import pytest

from netbot.input_expander import (
    INPUT_DROP,
    INPUT_LEFT,
    INPUT_RIGHT,
    INPUT_ROTATE,
    expand_placement,
)


# ---------------------------------------------------------------------------
# Hand-computed reference cases. Each entry is the *exact* sequence the C++
# port must produce for the same inputs — rotate steps first, then horizontal
# steps, then a single INPUT_DROP at the end.
# ---------------------------------------------------------------------------
@pytest.mark.parametrize(
    "cur_col, cur_rot, tgt_col, tgt_rot, expected",
    [
        # No-op rotation, no-op move -> just drop.
        (4, 0, 4, 0, [INPUT_DROP]),
        # Single rotation forward.
        (4, 0, 4, 1, [INPUT_ROTATE, INPUT_DROP]),
        # Rotation wrap — 3 -> 0 should take 1 forward rotation, not 3.
        # Wait: (0 - 3) % 4 == 1, so one ROTATE is enough. Lock this behavior.
        (4, 3, 4, 0, [INPUT_ROTATE, INPUT_DROP]),
        # 0 -> 3 takes three ROTATEs (Python side never goes backwards).
        (4, 0, 4, 3, [INPUT_ROTATE, INPUT_ROTATE, INPUT_ROTATE, INPUT_DROP]),
        # Pure rightward slide, no rotation.
        (0, 0, 3, 0, [INPUT_RIGHT, INPUT_RIGHT, INPUT_RIGHT, INPUT_DROP]),
        # Pure leftward slide.
        (6, 0, 2, 0, [INPUT_LEFT, INPUT_LEFT, INPUT_LEFT, INPUT_LEFT, INPUT_DROP]),
        # Rotation before translation — order is load-bearing.
        (4, 0, 6, 2, [
            INPUT_ROTATE, INPUT_ROTATE,
            INPUT_RIGHT, INPUT_RIGHT,
            INPUT_DROP,
        ]),
        # Rotation wrap combined with leftward slide.
        (5, 2, 1, 1, [
            INPUT_ROTATE,  INPUT_ROTATE,  INPUT_ROTATE,  # (1-2) % 4 == 3
            INPUT_LEFT, INPUT_LEFT, INPUT_LEFT, INPUT_LEFT,
            INPUT_DROP,
        ]),
    ],
)
def test_expand_placement_reference_table(
    cur_col: int, cur_rot: int, tgt_col: int, tgt_rot: int, expected: list[int]
) -> None:
    assert expand_placement(cur_col, cur_rot, tgt_col, tgt_rot) == expected


# ---------------------------------------------------------------------------
# Structural invariants: regardless of inputs, the output must (a) end with a
# single INPUT_DROP, (b) contain no mixed L/R within the same sequence, and
# (c) have all rotations before any horizontal move.
# ---------------------------------------------------------------------------
@pytest.mark.parametrize("cur_col", range(10))
@pytest.mark.parametrize("tgt_col", range(10))
@pytest.mark.parametrize("cur_rot", range(4))
@pytest.mark.parametrize("tgt_rot", range(4))
def test_expand_placement_invariants(
    cur_col: int, tgt_col: int, cur_rot: int, tgt_rot: int
) -> None:
    seq = expand_placement(cur_col, cur_rot, tgt_col, tgt_rot)

    # (a) ends with exactly one INPUT_DROP.
    assert seq[-1] == INPUT_DROP
    assert seq.count(INPUT_DROP) == 1

    # (b) horizontal moves are one-directional — never both LEFT and RIGHT.
    has_left = INPUT_LEFT in seq
    has_right = INPUT_RIGHT in seq
    assert not (has_left and has_right)

    # (c) every ROTATE precedes every LEFT/RIGHT.
    rotate_idxs = [i for i, m in enumerate(seq) if m == INPUT_ROTATE]
    move_idxs = [i for i, m in enumerate(seq) if m in (INPUT_LEFT, INPUT_RIGHT)]
    if rotate_idxs and move_idxs:
        assert max(rotate_idxs) < min(move_idxs)

    # Rotate count matches the forward-only wrap formula.
    assert len(rotate_idxs) == (tgt_rot - cur_rot) % 4

    # Horizontal step count matches |col delta|.
    assert len(move_idxs) == abs(tgt_col - cur_col)
