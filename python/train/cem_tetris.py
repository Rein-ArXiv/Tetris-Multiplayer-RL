"""Cross-Entropy Method trainer for the Tetris placement bot.

CEM is useful as a simple, robust policy-search baseline:

1. sample episodes from the current policy
2. keep the top ``elite_frac`` episodes by raw lines cleared
3. train ``TetrisPolicyNet`` by supervised cross-entropy on elite actions

The resulting checkpoint is the canonical policy network and exports directly
to ONNX.
"""

from __future__ import annotations

import argparse
import os
import sys
import time
from pathlib import Path
from typing import Any

import numpy as np
import torch
import torch.nn.functional as F
import torch.nn.utils as nn_utils

_PY_ROOT = Path(__file__).resolve().parent.parent
if str(_PY_ROOT) not in sys.path:
    sys.path.insert(0, str(_PY_ROOT))

from train.rl_common import evaluate_greedy, obs_to_batch
from common.checkpoint import load_checkpoint, save_checkpoint
from common.env import TetrisPlacementEnv
from common.models import TetrisPolicyNet, masked_log_softmax


@torch.no_grad()
def sample_policy_action(
    model: TetrisPolicyNet,
    obs: dict[str, np.ndarray],
    mask_np: np.ndarray,
    *,
    device: torch.device,
    temperature: float,
    epsilon: float,
) -> int:
    legal = np.flatnonzero(np.asarray(mask_np, dtype=bool))
    if legal.size == 0:
        raise RuntimeError("Cannot sample from an all-false legal mask.")
    if np.random.random() < epsilon:
        return int(np.random.choice(legal))

    batch = obs_to_batch(obs, device)
    mask = torch.as_tensor(mask_np, dtype=torch.bool, device=device).unsqueeze(0)
    logits, _value = model(batch["board"], batch["current"], batch["next"])
    logp = masked_log_softmax(logits / max(float(temperature), 1e-6), mask)
    action = torch.multinomial(logp.exp(), 1).squeeze(1)
    return int(action.item())


def collect_episode(
    model: TetrisPolicyNet,
    *,
    seed: int,
    args: argparse.Namespace,
    device: torch.device,
) -> dict[str, Any]:
    env = TetrisPlacementEnv(seed=seed)
    obs, info = env.reset(seed=seed)

    boards: list[np.ndarray] = []
    currents: list[np.ndarray] = []
    nexts: list[np.ndarray] = []
    masks: list[np.ndarray] = []
    actions: list[int] = []
    raw_lines = 0.0
    score = 0.0

    for _piece in range(args.max_pieces):
        mask = np.asarray(info["legal_mask"], dtype=bool)
        if not mask.any():
            break
        action = sample_policy_action(
            model,
            obs,
            mask,
            device=device,
            temperature=args.temperature,
            epsilon=args.epsilon,
        )
        boards.append(np.asarray(obs["board"], dtype=np.float32))
        currents.append(np.asarray(obs["current"], dtype=np.float32))
        nexts.append(np.asarray(obs["next"], dtype=np.float32))
        masks.append(mask.copy())
        actions.append(action)

        obs, reward, term, trunc, info = env.step(action)
        raw_lines += float(reward)
        score = float(info.get("score", score))
        if term or trunc:
            break

    return {
        "board": boards,
        "current": currents,
        "next": nexts,
        "mask": masks,
        "action": actions,
        "lines": raw_lines,
        "score": score,
        "pieces": len(actions),
    }


def flatten_elites(elites: list[dict[str, Any]]) -> dict[str, np.ndarray]:
    return {
        "board": np.stack([x for ep in elites for x in ep["board"]]).astype(np.float32),
        "current": np.stack([x for ep in elites for x in ep["current"]]).astype(np.float32),
        "next": np.stack([x for ep in elites for x in ep["next"]]).astype(np.float32),
        "mask": np.stack([x for ep in elites for x in ep["mask"]]).astype(np.bool_),
        "action": np.asarray([x for ep in elites for x in ep["action"]], dtype=np.int64),
        "return": np.asarray(
            [ep["lines"] for ep in elites for _ in ep["action"]],
            dtype=np.float32,
        ),
    }


def fit_elites(
    model: TetrisPolicyNet,
    opt: torch.optim.Optimizer,
    data: dict[str, np.ndarray],
    args: argparse.Namespace,
    *,
    device: torch.device,
) -> dict[str, float]:
    n = int(data["action"].shape[0])
    if n == 0:
        return {"ce": float("nan"), "value": float("nan")}

    boards = torch.as_tensor(data["board"], dtype=torch.float32, device=device)
    currents = torch.as_tensor(data["current"], dtype=torch.float32, device=device)
    nexts = torch.as_tensor(data["next"], dtype=torch.float32, device=device)
    masks = torch.as_tensor(data["mask"], dtype=torch.bool, device=device)
    actions = torch.as_tensor(data["action"], dtype=torch.long, device=device)
    returns_np = data["return"].astype(np.float32)
    returns_np = (returns_np - returns_np.mean()) / (returns_np.std() + 1e-6)
    returns = torch.as_tensor(returns_np, dtype=torch.float32, device=device)

    indices = np.arange(n)
    ce_total = 0.0
    value_total = 0.0
    updates = 0
    model.train()
    for _epoch in range(args.epochs):
        np.random.shuffle(indices)
        for start in range(0, n, args.batch):
            idx = torch.as_tensor(indices[start:start + args.batch], dtype=torch.long, device=device)
            logits, value = model(boards[idx], currents[idx], nexts[idx])
            loss_ce = F.cross_entropy(logits.masked_fill(~masks[idx], -1e9), actions[idx])
            loss_v = F.mse_loss(value, returns[idx])
            loss = loss_ce + args.value_coef * loss_v

            opt.zero_grad(set_to_none=True)
            loss.backward()
            nn_utils.clip_grad_norm_(model.parameters(), args.max_grad_norm)
            opt.step()

            ce_total += float(loss_ce.detach().item())
            value_total += float(loss_v.detach().item())
            updates += 1

    denom = max(1, updates)
    return {"ce": ce_total / denom, "value": value_total / denom}


def train(args: argparse.Namespace) -> None:
    device = torch.device(args.device)
    torch.manual_seed(args.seed)
    np.random.seed(args.seed)

    if args.resume and os.path.exists(args.resume):
        print(f"[cem] resuming from {args.resume}")
        model = load_checkpoint(args.resume, device=device)
        model.train()
    else:
        model = TetrisPolicyNet().to(device)
    opt = torch.optim.Adam(model.parameters(), lr=args.lr, eps=1e-5)
    out_path = Path(args.out)
    out_path.parent.mkdir(parents=True, exist_ok=True)

    best_eval_lines = -1e9
    t0 = time.time()
    for iteration in range(1, args.iterations + 1):
        model.eval()
        episodes = [
            collect_episode(
                model,
                seed=args.seed + iteration * 100_000 + ep,
                args=args,
                device=device,
            )
            for ep in range(args.episodes_per_iter)
        ]
        episodes = [ep for ep in episodes if ep["pieces"] > 0]
        if not episodes:
            raise RuntimeError("CEM collected no non-empty episodes.")

        episodes.sort(key=lambda ep: (ep["lines"], ep["score"], ep["pieces"]), reverse=True)
        elite_count = max(1, int(round(len(episodes) * args.elite_frac)))
        elites = episodes[:elite_count]
        data = flatten_elites(elites)
        stats = fit_elites(model, opt, data, args, device=device)

        elapsed = max(time.time() - t0, 1e-6)
        print(
            f"[cem] iter {iteration:>4}/{args.iterations} "
            f"elite {elite_count:>3}/{len(episodes):>3} "
            f"best_lines {episodes[0]['lines']:7.2f} "
            f"mean_lines {np.mean([ep['lines'] for ep in episodes]):7.2f} "
            f"ce {stats['ce']:.4f} v {stats['value']:.4f} "
            f"{iteration / elapsed:.2f} iter/s",
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
                        "algorithm": "cem",
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
                    "algorithm": "cem",
                    "iteration": iteration,
                    "elite_frac": args.elite_frac,
                },
            )

    save_checkpoint(
        model,
        out_path,
        extra={
            "algorithm": "cem",
            "iterations": args.iterations,
            "elite_frac": args.elite_frac,
        },
    )
    print(f"[cem] done. saved {out_path}")


def build_argparser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(description="Cross-Entropy Method for Tetris placement bot")
    p.add_argument("--iterations", type=int, default=50)
    p.add_argument("--episodes-per-iter", type=int, default=64)
    p.add_argument("--elite-frac", type=float, default=0.20)
    p.add_argument("--epochs", type=int, default=3)
    p.add_argument("--batch", type=int, default=256)
    p.add_argument("--lr", type=float, default=3e-4)
    p.add_argument("--value-coef", type=float, default=0.25)
    p.add_argument("--max-grad-norm", type=float, default=5.0)
    p.add_argument("--temperature", type=float, default=1.2)
    p.add_argument("--epsilon", type=float, default=0.05)
    p.add_argument("--max-pieces", type=int, default=2000)
    p.add_argument("--out", type=str, default="checkpoints/cem.pt")
    p.add_argument("--resume", type=str, default="")
    p.add_argument("--save-every", type=int, default=1)
    p.add_argument("--eval-every", type=int, default=5)
    p.add_argument("--eval-episodes", type=int, default=5)
    p.add_argument("--eval-max-pieces", type=int, default=5000)
    p.add_argument("--eval-seed", type=int, default=500_000)
    p.add_argument("--seed", type=int, default=42)
    p.add_argument("--device", type=str,
                   default="cuda" if torch.cuda.is_available() else "cpu")
    return p


if __name__ == "__main__":
    train(build_argparser().parse_args())
