"""Policy / value network definitions for the Tetris RL bot.

Both Colab training and the ONNX export CLI import the **same** ``TetrisPolicyNet``
class from this module — that's the contract that lets a checkpoint trained on
Colab Linux export cleanly for the C++ in-game bot.

``ARCH_VERSION`` is an explicit compatibility marker, not automatic schema
inference. Strict state loading checks keys/shapes; matching shapes alone cannot
prove that observation channels or action labels still mean the same thing.
"""

from __future__ import annotations

import torch
import torch.nn as nn
import torch.nn.functional as F

from .model_contract import positive_size, validate_policy_inputs

from . import BOARD_ROWS, BOARD_COLS, NUM_PIECE_TYPES, NUM_PLACEMENTS


class TetrisPolicyNet(nn.Module):
    """Shared spatial features with action scores and a scalar value head.

    Canonical input: board (B, board_channels, BOARD_ROWS, BOARD_COLS),
    current/next (B, n_piece_types). The normal observation path uses float32
    binary occupancy and piece-id-minus-one one-hot vectors. BHW is an optional
    single-channel batch alias, not a general unbatched CHW input.

    Outputs: action scores (B, n_placements), value (B,). Actor-critic training
    interprets the scores as policy logits; their meaning depends on the loss.
    Eager forward checks metadata against model dtype/device. Observation value
    validation belongs to the encoder; tracing captures only tensor operations.
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
        board_channels = positive_size(board_channels, "board_channels")
        hidden = positive_size(hidden, "hidden")
        n_placements = positive_size(n_placements, "n_placements")
        n_piece_types = positive_size(n_piece_types, "n_piece_types")
        if not isinstance(conv_channels, (tuple, list)) or not conv_channels:
            raise ValueError("conv_channels must be a nonempty sequence")
        conv_channels = tuple(positive_size(c, "conv_channels") for c in conv_channels)
        self.board_channels = board_channels
        self.conv_channels = conv_channels
        self.hidden = hidden
        self.n_placements = n_placements
        self.n_piece_types = n_piece_types

        # Stride 1 and padding 1 preserve the configured spatial axes.
        layers: list[nn.Module] = []
        in_ch = board_channels
        for out_ch in conv_channels:
            layers.append(nn.Conv2d(in_ch, out_ch, kernel_size=3, padding=1))
            layers.append(nn.ReLU(inplace=True))
            in_ch = out_ch
        self.trunk = nn.Sequential(*layers)

        flat = conv_channels[-1] * BOARD_ROWS * BOARD_COLS

        # conv가 뽑은 보드 특징에 현재/다음 블록 one-hot을 이어 붙인다.
        # 어떤 블록이 오는지 모르면 어디에 둘지 정할 수 없기 때문이다.
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
        if not isinstance(board, torch.Tensor):
            raise TypeError("board must be a tensor")
        if board.dim() == 3:
            board = board.unsqueeze(1)  # BHW -> NCHW; checked below in eager mode.
        if not torch.jit.is_tracing():
            validate_policy_inputs(
                board, current, next, channels=self.board_channels,
                rows=BOARD_ROWS, cols=BOARD_COLS, pieces=self.n_piece_types,
                parameter=self.trunk[0].weight,
            )
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
    """Normalize finite legal logits; reject empty legal rows and schema mismatch.

    ``eps`` remains accepted for caller compatibility. Adding the same constant
    cannot repair an empty distribution and is not a probability floor.
    """
    del eps
    if not logits.is_floating_point() or mask.dtype != torch.bool:
        raise TypeError("logits must be floating point and mask must be bool")
    if logits.ndim < 1 or logits.shape[-1] == 0 or logits.shape != mask.shape:
        raise ValueError("logits and mask must have the same nonempty action axis")
    if logits.device != mask.device:
        raise ValueError("logits and mask must be on the same device")
    if not mask.any(dim=-1).all():
        raise ValueError("each row must have at least one legal action")
    if not torch.isfinite(logits.masked_select(mask)).all():
        raise ValueError("legal logits must be finite")
    masked = logits.masked_fill(~mask, float("-inf"))
    logp = F.log_softmax(masked, dim=-1)
    if not torch.isfinite(logp.masked_select(mask)).all():
        raise ValueError("normalized legal log probabilities must be finite")
    return logp


def masked_entropy(logp: torch.Tensor, mask: torch.Tensor) -> torch.Tensor:
    """Entropy of masked_log_softmax output; avoid 0 * -inf before autograd."""
    safe_logp = logp.masked_fill(~mask, 0.0)
    return -(logp.exp() * safe_logp).sum(dim=-1)
