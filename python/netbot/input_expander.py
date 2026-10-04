"""Translate placement-level decisions into per-tick frame inputs.

The trained policy and rule-based baseline pick a target placement
``(col, rot)``, while the C++ game loop consumes one input bitmask per tick.
This bounded command encoder converts a placement into the same per-tick sequence
used by ``bot/placement.cpp`` (rotate -> translate -> hard-drop).

If a policy proposes an illegal placement, :func:`fallback_placement` returns
the first legal placement. Regression tests keep this module aligned with the
C++ implementation; the runtime in-process bot uses the C++ implementation.
"""

from __future__ import annotations

from typing import TYPE_CHECKING
from common import NUM_COLS, NUM_ROTATIONS
from .expansion_contract import validate_expansion

# core/input.h의 비트값을 그대로 베껴 쓴다. 값이 하나라도 어긋나면
# C++ bot과 Python이 같은 placement를 다른 키 입력으로 풀게 된다.
INPUT_NONE = 0
INPUT_LEFT = 1 << 0
INPUT_RIGHT = 1 << 1
INPUT_DOWN = 1 << 2
INPUT_ROTATE = 1 << 3
INPUT_DROP = 1 << 4

if TYPE_CHECKING:
    from sim import SimGame


def expand_placement(
    cur_col: int,
    cur_rot: int,
    tgt_col: int,
    tgt_rot: int,
    num_rotations: int = NUM_ROTATIONS,
) -> list[int]:
    """Encode rotate/translate/drop requests in the native command domain.

    This helper has no board or gravity state: it does not prove arrival.
    Invalid types/domains are rejected before arithmetic or allocation.

    The shared input protocol exposes clockwise rotation. The normalized
    difference chooses that many clockwise requests; collisions are checked
    by the live controller, not by this encoder.
    """
    cur_col, cur_rot, tgt_col, tgt_rot, num_rotations = validate_expansion(
        cur_col, cur_rot, tgt_col, tgt_rot, num_rotations, NUM_COLS, NUM_ROTATIONS)
    seq: list[int] = []

    rot_steps = (tgt_rot - cur_rot) % num_rotations
    for _ in range(rot_steps):
        seq.append(INPUT_ROTATE)

    if tgt_col > cur_col:
        bit = INPUT_RIGHT
    elif tgt_col < cur_col:
        bit = INPUT_LEFT
    else:
        bit = INPUT_NONE

    if bit != INPUT_NONE:
        for _ in range(abs(tgt_col - cur_col)):
            seq.append(bit)

    seq.append(INPUT_DROP)
    return seq


def fallback_placement(sim: "SimGame") -> tuple[int, int] | None:
    """Cheap fallback: pick the first legal placement (lowest col, lowest rot).

    This helper is opt-in; the encoder never calls it automatically.
    Native legal_placements checks endpoints, not every intervening key step.
    """
    placements = sim.legal_placements()
    if not placements:
        return None
    placements_sorted = sorted(placements, key=lambda p: (p.col, p.rot))
    p = placements_sorted[0]
    return p.col, p.rot


__all__ = [
    "INPUT_NONE",
    "INPUT_LEFT",
    "INPUT_RIGHT",
    "INPUT_DOWN",
    "INPUT_ROTATE",
    "INPUT_DROP",
    "expand_placement",
    "fallback_placement",
]
