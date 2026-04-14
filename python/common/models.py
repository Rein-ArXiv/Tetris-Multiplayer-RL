"""Policy / value network definitions for the Tetris RL bot.

Both Colab training and local netbot inference import the **same** ``TetrisPolicyNet``
class from this module — that's the contract that lets a checkpoint trained on
Colab Linux load cleanly into the Windows netbot.

If you change the architecture, **bump ``ARCH_VERSION``**. ``checkpoint.py``
verifies that the loaded checkpoint's recorded version matches the current
class, and raises a hard error otherwise. That way a silent shape mismatch
cannot turn into a confused-policy bug at game time.
"""

from __future__ import annotations

import torch
import torch.nn as nn
import torch.nn.functional as F

from . import BOARD_ROWS, BOARD_COLS, NUM_PIECE_TYPES, NUM_PLACEMENTS


class TetrisPolicyNet(nn.Module):
    """Shared trunk + policy/value heads.

    Input contract (matches ``common.obs.build_observation``)::

        board   : (B, 1, 20, 10) float32, occupancy in {0.0, 1.0}
        current : (B, 7) float32, one-hot of current piece id - 1
        next    : (B, 7) float32, one-hot of next piece id - 1

    Output::

        policy_logits : (B, 40) float32 — over (col * 4 + rot) placements
        value         : (B,)    float32 — scalar state value

    Bump ``ARCH_VERSION`` whenever any of the above shapes, the layer stack, or
    the layer ordering changes. The checkpoint loader treats a version mismatch
    as a hard failure.
    """

    ARCH_VERSION = 1

    def __init__(
        self,
        board_channels: int = 1,
        conv_channels: tuple[int, ...] = (32, 64, 64),
        hidden: int = 256,
        n_placements: int = NUM_PLACEMENTS,
        n_piece_types: int = NUM_PIECE_TYPES,
    ) -> None:
        super().__init__()
        self.board_channels = board_channels
        self.conv_channels = conv_channels
        self.hidden = hidden
        self.n_placements = n_placements
        self.n_piece_types = n_piece_types

        # ---- Convolutional trunk over the 20x10 board --------------------
        layers: list[nn.Module] = []
        in_ch = board_channels
        for out_ch in conv_channels:
            layers.append(nn.Conv2d(in_ch, out_ch, kernel_size=3, padding=1))
            layers.append(nn.ReLU(inplace=True))
            in_ch = out_ch
        self.trunk = nn.Sequential(*layers)

        flat = conv_channels[-1] * BOARD_ROWS * BOARD_COLS

        # ---- Fuse board features with current+next piece one-hots --------
        self.fuse = nn.Sequential(
            nn.Linear(flat + 2 * n_piece_types, hidden),
            nn.ReLU(inplace=True),
            nn.Linear(hidden, hidden),
            nn.ReLU(inplace=True),
        )

        self.policy_head = nn.Linear(hidden, n_placements)
        self.value_head = nn.Linear(hidden, 1)

    def forward(
        self,
        board: torch.Tensor,
        current: torch.Tensor,
        next: torch.Tensor,  # noqa: A002 - matches obs key name
    ) -> tuple[torch.Tensor, torch.Tensor]:
        if board.dim() == 3:
            board = board.unsqueeze(1)  # (B, 20, 10) -> (B, 1, 20, 10)
        h = self.trunk(board)
        h = h.flatten(1)
        h = torch.cat([h, current, next], dim=-1)
        h = self.fuse(h)
        policy_logits = self.policy_head(h)
        value = self.value_head(h).squeeze(-1)
        return policy_logits, value


def masked_log_softmax(
    logits: torch.Tensor, mask: torch.Tensor, eps: float = 1e-9
) -> torch.Tensor:
    """Apply a boolean legal-action ``mask`` to ``logits`` then log-softmax.

    Setting illegal logits to ``-inf`` makes their softmax probability zero,
    so sampling and ``argmax`` only ever pick legal placements.
    """
    masked = logits.masked_fill(~mask, float("-inf"))
    return F.log_softmax(masked + eps, dim=-1)
