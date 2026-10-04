"""Legal action masks for the placement-level action space.

The action domain has NUM_COLS * NUM_ROTATIONS labels, encoded as
``action_index = col * NUM_ROTATIONS + rot``. Labels describe the piece origin
and orientation, not a searched input route.

The mask marks entries returned by SimGame.legal_placements(). It retains
geometrically duplicate orientations. Probability of an equivalent outcome is
the sum over its labels; label entropy can include within-group uncertainty.
A policy must apply the mask to finite legal logits and handle an empty mask
before sampling. This endpoint mask does not prove tick-input reachability.
"""

from __future__ import annotations

from typing import TYPE_CHECKING

from . import NUM_PLACEMENTS, NUM_ROTATIONS

if TYPE_CHECKING:
    import torch
    from sim import SimGame


def encode_action(col: int, rot: int) -> int:
    """Map a ``(col, rot)`` placement in the valid domain to a flat index."""
    return col * NUM_ROTATIONS + rot


def decode_action(action: int) -> tuple[int, int]:
    """Inverse of :func:`encode_action`."""
    return action // NUM_ROTATIONS, action % NUM_ROTATIONS


def legal_mask(sim: "SimGame") -> torch.Tensor:
    """Boolean tensor of shape ``(NUM_PLACEMENTS,)``.

    ``True`` at index ``encode_action(col, rot)`` iff that placement is in
    ``sim.legal_placements()``. The result lives on CPU; move to the policy
    device at the call site.
    """
    import torch

    mask = torch.zeros(NUM_PLACEMENTS, dtype=torch.bool)
    for placement in sim.legal_placements():
        mask[encode_action(placement.col, placement.rot)] = True
    return mask
