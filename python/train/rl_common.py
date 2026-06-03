"""Shared helpers for the hand-rolled Colab training scripts.

The scripts in ``python/train`` intentionally train the canonical
``common.models.TetrisPolicyNet`` whenever the output is meant for deployment.
This module keeps the small pieces of glue in one place: import path setup,
numpy observation batching, legal-action masking, greedy evaluation, and a
simple replay buffer.
"""

from __future__ import annotations

import sys
from pathlib import Path
from typing import Any

import numpy as np
import torch
import torch.nn as nn


def ensure_python_root() -> None:
    """Make ``python/`` importable for both ``python -m`` and script runs."""
    py_root = Path(__file__).resolve().parent.parent
    if str(py_root) not in sys.path:
        sys.path.insert(0, str(py_root))


ensure_python_root()

from common import BOARD_COLS, BOARD_ROWS, NUM_PIECE_TYPES, NUM_PLACEMENTS  # noqa: E402
from common.env import TetrisPlacementEnv  # noqa: E402
from common.features import bcts_score  # noqa: E402
from common.models import TetrisPolicyNet  # noqa: E402


ObsDict = dict[str, np.ndarray]


def obs_to_batch(obs: ObsDict, device: torch.device | str) -> dict[str, torch.Tensor]:
    """Convert one env observation into a batch of size 1 on ``device``."""
    return {
        "board": torch.as_tensor(obs["board"], dtype=torch.float32, device=device).unsqueeze(0),
        "current": torch.as_tensor(obs["current"], dtype=torch.float32, device=device).unsqueeze(0),
        "next": torch.as_tensor(obs["next"], dtype=torch.float32, device=device).unsqueeze(0),
    }


def obs_arrays(obs: ObsDict) -> tuple[np.ndarray, np.ndarray, np.ndarray]:
    """Return normalized numpy arrays suitable for replay-buffer storage."""
    return (
        np.asarray(obs["board"], dtype=np.float32),
        np.asarray(obs["current"], dtype=np.float32),
        np.asarray(obs["next"], dtype=np.float32),
    )


def masked_argmax(logits: torch.Tensor, mask: torch.Tensor) -> torch.Tensor:
    """Argmax over legal actions only.

    ``logits`` can be shape ``(A,)`` or ``(B, A)``. ``mask`` must have the same
    trailing action dimension. Rows with an all-false mask are a caller bug.
    """
    return torch.argmax(logits.masked_fill(~mask, float("-inf")), dim=-1)


def masked_softmax_np(logits: torch.Tensor, mask_np: np.ndarray, temperature: float = 1.0) -> np.ndarray:
    """Return a CPU probability vector over legal actions only."""
    mask = np.asarray(mask_np, dtype=bool)
    out = np.zeros(mask.shape[0], dtype=np.float32)
    if not mask.any():
        return out

    temp = max(float(temperature), 1e-6)
    values = logits.detach().float().cpu().numpy() / temp
    legal = values[mask]
    legal = legal - np.max(legal)
    probs = np.exp(legal)
    probs /= max(float(probs.sum()), 1e-12)
    out[mask] = probs.astype(np.float32)
    return out


def bcts_shaped_reward(next_obs: ObsDict, lines: float, coef: float) -> float:
    """Line-clear reward plus optional BCTS board-feature shaping."""
    if coef == 0.0:
        return float(lines)
    board = np.asarray(next_obs["board"], dtype=np.float32).reshape(BOARD_ROWS, BOARD_COLS)
    return float(lines) + float(coef) * bcts_score(board, int(lines))


@torch.no_grad()
def greedy_action(
    model: TetrisPolicyNet,
    obs: ObsDict,
    legal_mask: np.ndarray,
    device: torch.device | str,
) -> int:
    """Pick the legal action with the largest policy logit/Q value."""
    mask_np = np.asarray(legal_mask, dtype=bool)
    if not mask_np.any():
        raise RuntimeError("Cannot choose an action from an all-false legal mask.")
    batch = obs_to_batch(obs, device)
    mask = torch.as_tensor(mask_np, dtype=torch.bool, device=device)
    logits, _value = model(batch["board"], batch["current"], batch["next"])
    return int(masked_argmax(logits.squeeze(0), mask).item())


@torch.no_grad()
def evaluate_greedy(
    model: TetrisPolicyNet,
    *,
    episodes: int,
    seed: int,
    device: torch.device | str,
    max_pieces: int,
) -> dict[str, float]:
    """Evaluate a policy by greedy legal placement on fixed seeds."""
    if episodes <= 0:
        return {
            "avg_lines": float("nan"),
            "avg_score": float("nan"),
            "avg_pieces": float("nan"),
            "episodes": 0.0,
        }

    was_training = model.training
    model.eval()
    env = TetrisPlacementEnv(seed=seed)

    total_lines = 0.0
    total_score = 0.0
    total_pieces = 0.0
    for ep in range(episodes):
        obs, info = env.reset(seed=seed + ep)
        lines = 0.0
        score = 0.0
        pieces = 0
        while pieces < max_pieces:
            mask_np = np.asarray(info["legal_mask"], dtype=bool)
            if not mask_np.any():
                break
            action = greedy_action(model, obs, mask_np, device)
            obs, reward, term, trunc, info = env.step(action)
            lines += float(reward)
            score = float(info.get("score", score))
            pieces += 1
            if term or trunc:
                break
        total_lines += lines
        total_score += score
        total_pieces += float(pieces)

    if was_training:
        model.train()

    denom = float(episodes)
    return {
        "avg_lines": total_lines / denom,
        "avg_score": total_score / denom,
        "avg_pieces": total_pieces / denom,
        "episodes": denom,
    }


def soft_update(target: nn.Module, source: nn.Module, tau: float) -> None:
    """Polyak update target parameters in-place."""
    with torch.no_grad():
        for tgt, src in zip(target.parameters(), source.parameters(), strict=True):
            tgt.mul_(1.0 - tau).add_(src, alpha=tau)


class LinearSchedule:
    """Linear interpolation between two scalar values."""

    def __init__(self, start: float, end: float, duration: int) -> None:
        self.start = float(start)
        self.end = float(end)
        self.duration = max(int(duration), 1)

    def value(self, step: int) -> float:
        frac = min(max(float(step) / float(self.duration), 0.0), 1.0)
        return self.start + frac * (self.end - self.start)


class ReplayBuffer:
    """Fixed-size numpy replay buffer for placement-level transitions."""

    def __init__(self, capacity: int, n_actions: int = NUM_PLACEMENTS) -> None:
        if capacity <= 0:
            raise ValueError("ReplayBuffer capacity must be positive.")
        self.capacity = int(capacity)
        self.n_actions = int(n_actions)

        self.board = np.zeros((capacity, 1, BOARD_ROWS, BOARD_COLS), dtype=np.float32)
        self.current = np.zeros((capacity, NUM_PIECE_TYPES), dtype=np.float32)
        self.next_piece = np.zeros((capacity, NUM_PIECE_TYPES), dtype=np.float32)
        self.mask = np.zeros((capacity, n_actions), dtype=np.bool_)

        self.action = np.zeros((capacity,), dtype=np.int64)
        self.reward = np.zeros((capacity,), dtype=np.float32)
        self.done = np.zeros((capacity,), dtype=np.bool_)

        self.next_board = np.zeros((capacity, 1, BOARD_ROWS, BOARD_COLS), dtype=np.float32)
        self.next_current = np.zeros((capacity, NUM_PIECE_TYPES), dtype=np.float32)
        self.next_next_piece = np.zeros((capacity, NUM_PIECE_TYPES), dtype=np.float32)
        self.next_mask = np.zeros((capacity, n_actions), dtype=np.bool_)

        self.policy_target = np.zeros((capacity, n_actions), dtype=np.float32)
        self.value_target = np.zeros((capacity,), dtype=np.float32)

        self.pos = 0
        self.size = 0

    def __len__(self) -> int:
        return self.size

    def add(
        self,
        obs: ObsDict,
        mask: np.ndarray,
        action: int,
        reward: float,
        done: bool,
        next_obs: ObsDict,
        next_mask: np.ndarray,
        *,
        policy_target: np.ndarray | None = None,
        value_target: float = 0.0,
    ) -> None:
        i = self.pos
        board, current, next_piece = obs_arrays(obs)
        n_board, n_current, n_next_piece = obs_arrays(next_obs)

        self.board[i] = board
        self.current[i] = current
        self.next_piece[i] = next_piece
        self.mask[i] = np.asarray(mask, dtype=np.bool_)
        self.action[i] = int(action)
        self.reward[i] = float(reward)
        self.done[i] = bool(done)
        self.next_board[i] = n_board
        self.next_current[i] = n_current
        self.next_next_piece[i] = n_next_piece
        self.next_mask[i] = np.asarray(next_mask, dtype=np.bool_)

        if policy_target is None:
            self.policy_target[i].fill(0.0)
            if 0 <= int(action) < self.n_actions:
                self.policy_target[i, int(action)] = 1.0
        else:
            target = np.asarray(policy_target, dtype=np.float32)
            if target.shape != (self.n_actions,):
                raise ValueError(f"policy_target must have shape {(self.n_actions,)}, got {target.shape}")
            self.policy_target[i] = target
        self.value_target[i] = float(value_target)

        self.pos = (self.pos + 1) % self.capacity
        self.size = min(self.size + 1, self.capacity)

    def sample(self, batch_size: int, device: torch.device | str) -> dict[str, Any]:
        if self.size <= 0:
            raise RuntimeError("Cannot sample from an empty replay buffer.")
        idx = np.random.randint(0, self.size, size=int(batch_size))
        return {
            "board": torch.as_tensor(self.board[idx], dtype=torch.float32, device=device),
            "current": torch.as_tensor(self.current[idx], dtype=torch.float32, device=device),
            "next": torch.as_tensor(self.next_piece[idx], dtype=torch.float32, device=device),
            "mask": torch.as_tensor(self.mask[idx], dtype=torch.bool, device=device),
            "action": torch.as_tensor(self.action[idx], dtype=torch.long, device=device),
            "reward": torch.as_tensor(self.reward[idx], dtype=torch.float32, device=device),
            "done": torch.as_tensor(self.done[idx], dtype=torch.float32, device=device),
            "next_board": torch.as_tensor(self.next_board[idx], dtype=torch.float32, device=device),
            "next_current": torch.as_tensor(self.next_current[idx], dtype=torch.float32, device=device),
            "next_next": torch.as_tensor(self.next_next_piece[idx], dtype=torch.float32, device=device),
            "next_mask": torch.as_tensor(self.next_mask[idx], dtype=torch.bool, device=device),
            "policy_target": torch.as_tensor(self.policy_target[idx], dtype=torch.float32, device=device),
            "value_target": torch.as_tensor(self.value_target[idx], dtype=torch.float32, device=device),
        }
