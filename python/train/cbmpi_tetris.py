"""CBMPI-style trainer for the Tetris placement bot.

CBMPI (Classification-Based Modified Policy Iteration) alternates between:

1. policy improvement: score legal actions from the current state
2. policy fitting: train a classifier to imitate the improved action

For this project the improvement step uses real one-step ``SimGame`` clones,
then scores post-placement boards with BCTS features and an optional learned
value bootstrap. The fitted model is the canonical ``TetrisPolicyNet``, so the
saved checkpoint exports directly to ONNX.

Run from ``python/`` after the Colab setup notebook builds ``tetris_py``::

    python -m train.cbmpi_tetris --iterations 20 --out checkpoints/cbmpi.pt
    python -m netbot.export_onnx checkpoints/cbmpi.eval_best.pt ../model/cbmpi.onnx
"""

from __future__ import annotations

import argparse
import os
import sys
import time
from pathlib import Path

import numpy as np
import torch
import torch.nn.functional as F
import torch.nn.utils as nn_utils

_PY_ROOT = Path(__file__).resolve().parent.parent
if str(_PY_ROOT) not in sys.path:
    sys.path.insert(0, str(_PY_ROOT))

from train.rl_common import evaluate_greedy, greedy_action, obs_to_batch
from common import BOARD_COLS, BOARD_ROWS
from common.action_mask import encode_action
from common.checkpoint import load_checkpoint, save_checkpoint
from common.env import TetrisPlacementEnv
from common.features import bcts_score
from common.models import TetrisPolicyNet
from common.obs import build_observation


def _sim_clone(sim):
    if not hasattr(sim, "clone"):
        raise RuntimeError(
            "This CBMPI trainer requires SimGame.clone(). Rebuild tetris_py from "
            "the current repo in Colab with -DTETRIS_BUILD_PY=ON."
        )
    return sim.clone()


def obs_from_sim(sim) -> dict[str, np.ndarray]:
    obs_t = build_observation(sim)
    return {k: v.numpy() for k, v in obs_t.items()}


@torch.no_grad()
def bootstrap_value(
    model: TetrisPolicyNet,
    sim,
    *,
    device: torch.device,
) -> float:
    if sim.game_over():
        return 0.0
    obs = obs_from_sim(sim)
    batch = obs_to_batch(obs, device)
    _logits, value = model(batch["board"], batch["current"], batch["next"])
    return float(value.squeeze(0).item())


@torch.no_grad()
def improve_action(
    sim,
    model: TetrisPolicyNet,
    *,
    device: torch.device,
    bcts_coef: float,
    value_weight: float,
    line_weight: float,
) -> tuple[int, float]:
    """Return the best legal action and its scalar improvement score."""
    placements = sim.legal_placements()
    if not placements:
        raise RuntimeError("Cannot improve an all-terminal/no-legal state.")

    best_action = -1
    best_score = -float("inf")
    for placement in placements:
        child = _sim_clone(sim)
        cleared = int(child.apply_placement(placement.col, placement.rot))
        if cleared < 0:
            continue
        board = np.asarray(obs_from_sim(child)["board"], dtype=np.float32).reshape(BOARD_ROWS, BOARD_COLS)
        value_bootstrap = 0.0
        if value_weight != 0.0:
            value_bootstrap = bootstrap_value(model, child, device=device)
        score = (
            line_weight * float(cleared)
            + bcts_coef * bcts_score(board, cleared)
            + value_weight * value_bootstrap
        )
        action = encode_action(int(placement.col), int(placement.rot))
        if score > best_score:
            best_score = score
            best_action = action

    if best_action < 0:
        raise RuntimeError("All placements were rejected as illegal during improvement.")
    return best_action, best_score


def choose_rollout_action(
    model: TetrisPolicyNet,
    obs: dict[str, np.ndarray],
    mask: np.ndarray,
    improved_action: int,
    *,
    expert_mix: float,
    epsilon: float,
    device: torch.device,
) -> int:
    legal = np.flatnonzero(np.asarray(mask, dtype=bool))
    if legal.size == 0:
        raise RuntimeError("Cannot rollout from an all-false legal mask.")
    r = np.random.random()
    if r < expert_mix:
        return int(improved_action)
    if r < expert_mix + epsilon:
        return int(np.random.choice(legal))
    return greedy_action(model, obs, mask, device)


def collect_dataset(
    model: TetrisPolicyNet,
    args: argparse.Namespace,
    *,
    iteration: int,
    device: torch.device,
) -> dict[str, np.ndarray]:
    """Collect improved-action labels by rolling one env and cloning states."""
    env = TetrisPlacementEnv(seed=args.seed + iteration * 100_000)
    obs, info = env.reset(seed=args.seed + iteration * 100_000)

    boards: list[np.ndarray] = []
    currents: list[np.ndarray] = []
    nexts: list[np.ndarray] = []
    masks: list[np.ndarray] = []
    labels: list[int] = []
    values: list[float] = []

    ep_lines = 0.0
    ep_len = 0
    episodes = 0
    t0 = time.time()

    while len(labels) < args.states_per_iter:
        mask = np.asarray(info["legal_mask"], dtype=bool)
        if env.sim is None or not mask.any():
            obs, info = env.reset()
            ep_lines = 0.0
            ep_len = 0
            continue

        label, score = improve_action(
            env.sim,
            model,
            device=device,
            bcts_coef=args.bcts_coef,
            value_weight=args.value_weight,
            line_weight=args.line_weight,
        )

        boards.append(np.asarray(obs["board"], dtype=np.float32))
        currents.append(np.asarray(obs["current"], dtype=np.float32))
        nexts.append(np.asarray(obs["next"], dtype=np.float32))
        masks.append(mask.copy())
        labels.append(label)
        values.append(score)

        action = choose_rollout_action(
            model,
            obs,
            mask,
            label,
            expert_mix=args.expert_mix,
            epsilon=args.epsilon,
            device=device,
        )
        obs, reward, term, trunc, info = env.step(action)
        ep_lines += float(reward)
        ep_len += 1
        if term or trunc or ep_len >= args.max_pieces:
            episodes += 1
            obs, info = env.reset()
            ep_lines = 0.0
            ep_len = 0

    elapsed = max(time.time() - t0, 1e-6)
    print(
        f"[cbmpi] collected {len(labels)} states in {elapsed:.1f}s "
        f"({int(len(labels) / elapsed)} state/s, episodes {episodes})",
        flush=True,
    )

    return {
        "board": np.stack(boards).astype(np.float32),
        "current": np.stack(currents).astype(np.float32),
        "next": np.stack(nexts).astype(np.float32),
        "mask": np.stack(masks).astype(np.bool_),
        "label": np.asarray(labels, dtype=np.int64),
        "value": np.asarray(values, dtype=np.float32),
    }


def fit_policy(
    model: TetrisPolicyNet,
    opt: torch.optim.Optimizer,
    data: dict[str, np.ndarray],
    args: argparse.Namespace,
    *,
    device: torch.device,
) -> dict[str, float]:
    model.train()
    n = int(data["label"].shape[0])
    value_np = data["value"].astype(np.float32)
    value_mean = float(value_np.mean())
    value_std = float(value_np.std() + 1e-6)
    value_np = (value_np - value_mean) / value_std

    boards = torch.as_tensor(data["board"], dtype=torch.float32, device=device)
    currents = torch.as_tensor(data["current"], dtype=torch.float32, device=device)
    nexts = torch.as_tensor(data["next"], dtype=torch.float32, device=device)
    masks = torch.as_tensor(data["mask"], dtype=torch.bool, device=device)
    labels = torch.as_tensor(data["label"], dtype=torch.long, device=device)
    values = torch.as_tensor(value_np, dtype=torch.float32, device=device)

    ce_total = 0.0
    v_total = 0.0
    batches = 0
    indices = np.arange(n)
    for _epoch in range(args.epochs):
        np.random.shuffle(indices)
        for start in range(0, n, args.batch):
            idx = torch.as_tensor(indices[start:start + args.batch], dtype=torch.long, device=device)
            logits, value = model(boards[idx], currents[idx], nexts[idx])
            masked_logits = logits.masked_fill(~masks[idx], -1e9)
            ce = F.cross_entropy(masked_logits, labels[idx])
            v_loss = F.mse_loss(value, values[idx])
            loss = ce + args.value_loss_coef * v_loss

            opt.zero_grad(set_to_none=True)
            loss.backward()
            nn_utils.clip_grad_norm_(model.parameters(), args.max_grad_norm)
            opt.step()

            ce_total += float(ce.detach().item())
            v_total += float(v_loss.detach().item())
            batches += 1

    return {
        "ce": ce_total / max(1, batches),
        "value_loss": v_total / max(1, batches),
        "value_mean": value_mean,
        "value_std": value_std,
    }


def train(args: argparse.Namespace) -> None:
    device = torch.device(args.device)
    torch.manual_seed(args.seed)
    np.random.seed(args.seed)

    if args.resume and os.path.exists(args.resume):
        print(f"[cbmpi] resuming from {args.resume}")
        model = load_checkpoint(args.resume, device=device)
        model.train()
    else:
        model = TetrisPolicyNet().to(device)
    opt = torch.optim.Adam(model.parameters(), lr=args.lr, eps=1e-5)

    out_path = Path(args.out)
    out_path.parent.mkdir(parents=True, exist_ok=True)
    best_eval_lines = -1e9

    for iteration in range(1, args.iterations + 1):
        data = collect_dataset(model, args, iteration=iteration, device=device)
        stats = fit_policy(model, opt, data, args, device=device)
        print(
            f"[cbmpi] iter {iteration:>4}/{args.iterations} "
            f"ce {stats['ce']:.4f} value {stats['value_loss']:.4f} "
            f"target_mu {stats['value_mean']:.3f} target_sigma {stats['value_std']:.3f}",
            flush=True,
        )

        if args.eval_every > 0 and args.eval_episodes > 0 and iteration % args.eval_every == 0:
            eval_stats = evaluate_greedy(
                model,
                episodes=args.eval_episodes,
                seed=args.eval_seed,
                device=device,
                max_pieces=args.eval_max_pieces,
            )
            print(
                f"[eval] iter {iteration:>4} avg_lines {eval_stats['avg_lines']:8.2f} "
                f"avg_score {eval_stats['avg_score']:9.1f} "
                f"avg_pieces {eval_stats['avg_pieces']:8.1f}",
                flush=True,
            )
            if eval_stats["avg_lines"] > best_eval_lines:
                best_eval_lines = eval_stats["avg_lines"]
                save_checkpoint(
                    model,
                    out_path.with_suffix(".eval_best.pt"),
                    extra={
                        "algorithm": "cbmpi",
                        "iteration": iteration,
                        "eval_avg_lines": eval_stats["avg_lines"],
                        "eval_avg_score": eval_stats["avg_score"],
                        "eval_avg_pieces": eval_stats["avg_pieces"],
                    },
                )

        if args.save_every > 0 and iteration % args.save_every == 0:
            save_checkpoint(
                model,
                out_path,
                extra={
                    "algorithm": "cbmpi",
                    "iteration": iteration,
                    "states_per_iter": args.states_per_iter,
                    "bcts_coef": args.bcts_coef,
                    "value_weight": args.value_weight,
                },
            )

    save_checkpoint(
        model,
        out_path,
        extra={
            "algorithm": "cbmpi",
            "iterations": args.iterations,
            "states_per_iter": args.states_per_iter,
            "bcts_coef": args.bcts_coef,
            "value_weight": args.value_weight,
        },
    )
    print(f"[cbmpi] done. saved {out_path}")


def build_argparser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(description="CBMPI-style trainer for Tetris placement bot")
    p.add_argument("--iterations", type=int, default=20)
    p.add_argument("--states-per-iter", type=int, default=20_000)
    p.add_argument("--epochs", type=int, default=3)
    p.add_argument("--batch", type=int, default=256)
    p.add_argument("--lr", type=float, default=3e-4)
    p.add_argument("--line-weight", type=float, default=1.0)
    p.add_argument("--bcts-coef", type=float, default=1.0)
    p.add_argument("--value-weight", type=float, default=0.0,
                   help="bootstrap weight for the current network value head")
    p.add_argument("--value-loss-coef", type=float, default=0.25)
    p.add_argument("--expert-mix", type=float, default=0.80,
                   help="probability of rolling out with the improved action")
    p.add_argument("--epsilon", type=float, default=0.05,
                   help="additional random exploration probability")
    p.add_argument("--max-pieces", type=int, default=5000)
    p.add_argument("--max-grad-norm", type=float, default=5.0)
    p.add_argument("--out", type=str, default="checkpoints/cbmpi.pt")
    p.add_argument("--resume", type=str, default="")
    p.add_argument("--save-every", type=int, default=1)
    p.add_argument("--eval-every", type=int, default=1)
    p.add_argument("--eval-episodes", type=int, default=5)
    p.add_argument("--eval-max-pieces", type=int, default=5000)
    p.add_argument("--eval-seed", type=int, default=300_000)
    p.add_argument("--seed", type=int, default=42)
    p.add_argument("--device", type=str,
                   default="cuda" if torch.cuda.is_available() else "cpu")
    return p


if __name__ == "__main__":
    train(build_argparser().parse_args())
