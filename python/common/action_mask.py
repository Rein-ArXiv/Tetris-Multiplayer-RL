"""Legal action masks for the placement-level action space.

The action space is fixed at ``NUM_PLACEMENTS == NUM_COLS * NUM_ROTATIONS == 40``,
encoded as ``action_index = col * NUM_ROTATIONS + rot``. Some pieces have fewer
than 4 unique rotations and some columns put the piece out of bounds; the legal
mask zeros those out so the policy can never sample them.
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
