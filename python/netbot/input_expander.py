"""Translate placement-level decisions into per-tick frame inputs.

The trained policy and rule-based baseline pick a target placement
``(col, rot)``, while the C++ game loop consumes one input bitmask per tick.
This parity helper converts a placement into the same single-tick sequence
used by ``bot/placement.cpp`` (rotate -> translate -> hard-drop).

If a policy proposes an illegal placement, :func:`fallback_placement` returns
the first legal placement. Regression tests keep this module aligned with the
C++ implementation; the runtime in-process bot uses the C++ implementation.
"""

from __future__ import annotations

from typing import TYPE_CHECKING

# Mirror of core/input.h for the Python/C++ placement parity layer.
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
    num_rotations: int = 4,
) -> list[int]:
    """Build a frame-mask sequence that walks ``(cur_col, cur_rot)`` to
    ``(tgt_col, tgt_rot)`` and then hard drops.

    Rotations always go forward (the C++ block class only has ``Rotate`` /
    ``UndoRotation`` and rotation is the cheap operation, so 1-3 rotates is
    fine even if 1 backwards rotate would be shorter).
    """
    if num_rotations <= 0:
        raise ValueError(f"num_rotations must be positive, got {num_rotations}")
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

    Used when the chosen placement's expanded sequence fails validation, or
    when the policy returns an action whose mask bit is False (which the
    masking layer should prevent, but defensive code costs nothing here).
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
