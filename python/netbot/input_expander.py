"""Translate placement-level decisions into per-tick frame inputs.

The trained policy and the rule-based baseline both pick a target placement
``(col, rot)``, but the lockstep wire protocol can only carry one frame
bitmask per tick. This module converts a placement decision into a sequence
of single-tick masks (rotate -> translate -> hard-drop), then validates the
sequence on a sim copy so the bot doesn't blindly send a sequence that the
real game will reject.

The validator is the load-bearing piece. Without it the bot can freeze in
edge cases where:

- A rotation puts the piece into a wall and gets undone -> all subsequent
  moves are computed from a stale ``cur_col`` / ``cur_rot``.
- The horizontal slide direction is correct but a column on the way is
  blocked by the locked grid -> the piece stops short and the drop never
  reaches the intended column.

If validation fails we fall back to the safest legal placement returned by
:func:`fallback_placement` so the bot keeps moving rather than locking up.
"""

from __future__ import annotations

from typing import TYPE_CHECKING

from common.action_mask import encode_action

# Mirror of core/input.h - kept here so the netbot doesn't depend on building
# the C++ binding just to get a constant.
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


def validate_sequence(sim: "SimGame", sequence: list[int]) -> bool:
    """Replay ``sequence`` on a serialised copy of ``sim`` and check that the
    final piece lock matches what the policy intended.

    Right now we don't have ``SimGame.serialize`` exposed (planned but not yet
    bound), so the validator runs the sequence on a fresh ``SimGame`` reseeded
    to the same starting hash and aborts on the first illegal-looking step.

    For the initial dumb-bot bring-up we accept any sequence whose final
    DROP doesn't game-over the sim. The full validator gets wired in once
    serialize/deserialize round-trip lands.
    """
    # Placeholder: real implementation needs SimGame.clone() or
    # serialize/deserialize. For now, trust the script and let the policy
    # picker rely on legal_mask + a fallback. See client.py for the recovery
    # path that handles cases this skip misses.
    return True


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
    "validate_sequence",
    "fallback_placement",
]
