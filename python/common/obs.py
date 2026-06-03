"""SimGame -> observation tensor builder.

This module is the **only** place that converts a ``SimGame`` snapshot into a
network input. Both the Colab training rollouts and the local netbot inference
loop call ``build_observation`` — keeping the conversion in one spot is what
prevents the classic "trained on one obs format, deployed on a slightly
different one, model outputs garbage" failure mode.

The schema:

- ``board``   : float32 (1, 20, 10), 1 where the cell is occupied by a locked
  piece, 0 otherwise. The currently falling piece and the ghost preview are
  excluded — the policy reasons about *committed* state plus the piece id.
- ``current`` : float32 (7,), one-hot of ``current_block_id - 1``
- ``next``    : float32 (7,), one-hot of ``next_block_id - 1``

The ghost block id is 8 in the C++ sim, and the ghost lives directly on the
grid layout in *some* paths but not in ``SimGrid::grid`` (locked cells only).
We treat any cell with value ``> 0 and != 8`` as occupied to be defensive.
"""

from __future__ import annotations

from typing import TYPE_CHECKING

import numpy as np

from . import NUM_PIECE_TYPES

if TYPE_CHECKING:
    import torch
    from sim import SimGame


def build_observation(sim: "SimGame") -> dict[str, torch.Tensor]:
    """Convert a ``SimGame`` snapshot into the dict consumed by ``TetrisPolicyNet``.

    Returns un-batched tensors. Add a leading batch dim with ``unsqueeze(0)``
    before passing to the network — done at the call site so that batched
    rollouts and single-step inference share this builder.
    """
    import torch

    raw = np.asarray(sim.grid(), dtype=np.float32)  # (20, 10)
    occupied = ((raw > 0) & (raw != 8)).astype(np.float32)
    board = occupied[None, :, :]  # (1, 20, 10)

    current = _piece_one_hot(sim.current_block_id())
    nxt = _piece_one_hot(sim.next_block_id())

    return {
        "board": torch.from_numpy(board),
        "current": torch.from_numpy(current),
        "next": torch.from_numpy(nxt),
    }


def _piece_one_hot(piece_id: int) -> np.ndarray:
    """One-hot encode a piece id (1..7) into a length-7 float32 vector."""
    out = np.zeros(NUM_PIECE_TYPES, dtype=np.float32)
    if 1 <= piece_id <= NUM_PIECE_TYPES:
        out[piece_id - 1] = 1.0
    return out
