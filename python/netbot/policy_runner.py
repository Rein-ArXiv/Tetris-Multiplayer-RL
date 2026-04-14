"""Action selectors for the netbot.

Two implementations share a tiny interface (``select_placement(sim) -> (col, rot)``):

- :class:`PolicyRunner`     — loads a trained checkpoint via ``common.checkpoint``
                              and picks ``argmax`` over masked policy logits.
- :class:`RuleBasedRunner`  — Dellacherie/BCTS linear scoring (no checkpoint
                              needed). Useful as a sanity baseline and as the
                              netbot's behaviour when no policy is supplied.

Both runners always return a placement that's in ``sim.legal_placements()``.
"""

from __future__ import annotations

from typing import TYPE_CHECKING, Protocol

import numpy as np
import torch

from common.action_mask import decode_action, legal_mask
from common.checkpoint import load_checkpoint
from common.features import bcts_score
from common.obs import build_observation

if TYPE_CHECKING:
    from sim import SimGame


class Runner(Protocol):
    def select_placement(self, sim: "SimGame") -> tuple[int, int]: ...


class PolicyRunner:
    """Argmax over masked policy logits from a trained ``TetrisPolicyNet``."""

    def __init__(self, checkpoint_path: str, device: str = "cpu") -> None:
        self.device = device
        self.model = load_checkpoint(checkpoint_path, device=device)
        self.model.eval()

    @torch.no_grad()
    def select_placement(self, sim: "SimGame") -> tuple[int, int]:
        obs = build_observation(sim)
        batched = {k: v.unsqueeze(0).to(self.device) for k, v in obs.items()}
        logits, _value = self.model(**batched)
        mask = legal_mask(sim).to(self.device)
        masked_logits = logits.squeeze(0).masked_fill(~mask, float("-inf"))
        if torch.isinf(masked_logits).all():
            # No legal actions — caller should treat this as game over.
            return -1, -1
        action = int(torch.argmax(masked_logits).item())
        return decode_action(action)


class RuleBasedRunner:
    """Dellacherie / BCTS linear scoring over every legal placement.

    For each candidate placement we ask the sim to enumerate it (which already
    gives us the (col, rot) tuple), then we'd ideally simulate the placement
    on a clone, score the resulting board, and pick the best. Cloning is not
    yet bound, so for the initial bring-up we score the board *before* the
    placement and add a small bonus for placements that align with the
    deepest column — enough for "obviously decent" play.

    Once ``SimGame.clone()`` (or serialize/deserialize) lands, replace the
    body of :meth:`select_placement` with a true 1-ply lookahead.
    """

    def select_placement(self, sim: "SimGame") -> tuple[int, int]:
        placements = sim.legal_placements()
        if not placements:
            return -1, -1

        board = np.asarray(sim.grid(), dtype=np.float32)
        base_score = bcts_score(board, rows_cleared=0)

        # Heuristic: prefer placements that target the lowest current column
        # (the deepest hole). This is a placeholder until 1-ply lookahead is
        # available — but it already produces visibly non-random play.
        col_heights = _col_heights(board)
        deepest_col = int(np.argmin(col_heights))

        def score(p) -> float:
            col_distance = abs(p.col - deepest_col)
            return base_score - 0.1 * col_distance

        best = max(placements, key=score)
        return best.col, best.rot


def _col_heights(board: np.ndarray) -> np.ndarray:
    occupied = board > 0
    rows = board.shape[0]
    first_filled = np.where(
        occupied.any(axis=0),
        occupied.argmax(axis=0),
        rows,
    )
    return rows - first_filled
