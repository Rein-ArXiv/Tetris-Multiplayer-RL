"""SimGame -> observation tensor builder.

This module is the **only** Python place that converts a ``SimGame`` snapshot
into a network input. The C++ in-game bot's ``observe()`` (bot/placement.cpp)
mirrors this contract. ``test_observation_parity.py`` compares actual CPU
observations from both paths. Schema changes must update both implementations;
model input and ONNX integration checks remain separate.

Schema (unbatched): board float32 (1, BOARD_ROWS, BOARD_COLS), current and next
float32 (NUM_PIECE_TYPES,). The board contains locked occupancy only; current
and first-preview ids use the configured id-minus-one vocabulary order.

Active coordinates, remaining previews, timing, pending garbage and hidden RNG
state are omitted. This feature projection is not a full Markov state. The
legacy occupancy predicate also excludes display id 8, although SimGame.Grid()
itself contains locked cells rather than a rendered ghost.
"""

from __future__ import annotations

from typing import TYPE_CHECKING

import numpy as np

from . import NUM_PIECE_TYPES, BOARD_ROWS, BOARD_COLS

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

    raw = np.asarray(sim.grid(), dtype=np.float32)
    if raw.shape != (BOARD_ROWS, BOARD_COLS):
        raise ValueError(f"board shape {raw.shape} does not match {(BOARD_ROWS, BOARD_COLS)}")
    occupied = ((raw > 0) & (raw != 8)).astype(np.float32)
    board = occupied[None, :, :]  # channel, row, column; no batch axis yet

    current = _piece_one_hot(sim.current_block_id())
    nxt = _piece_one_hot(sim.next_block_id())

    return {
        "board": torch.from_numpy(board),
        "current": torch.from_numpy(current),
        "next": torch.from_numpy(nxt),
    }


def _piece_one_hot(piece_id: int) -> np.ndarray:
    """Encode the configured id order; unknown ids retain the legacy all-zero vector."""
    out = np.zeros(NUM_PIECE_TYPES, dtype=np.float32)
    if 1 <= piece_id <= NUM_PIECE_TYPES:
        out[piece_id - 1] = 1.0
    return out
