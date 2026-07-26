"""Legal action masks for the placement-level action space.

The action space is fixed at ``NUM_PLACEMENTS == NUM_COLS * NUM_ROTATIONS == 40``,
encoded as ``action_index = col * NUM_ROTATIONS + rot``.

The mask zeros out placements that are out of bounds or blocked, so the policy
can never sample an illegal move. It does **not** deduplicate: a piece whose
rotations are not all distinct (O has one shape, I/S/Z have two) keeps every
rotation index that lands legally, so the same resulting board can be reachable
through more than one action. That dilutes the policy distribution slightly but
keeps the action index identical on both sides of the pybind11 boundary.
"""

from __future__ import annotations

from typing import TYPE_CHECKING

from . import NUM_PLACEMENTS, NUM_ROTATIONS

if TYPE_CHECKING:
    import torch
    from sim import SimGame


def encode_action(col: int, rot: int) -> int:
    """Map a ``(col, rot)`` placement to a flat action index in ``[0, 40)``."""
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
