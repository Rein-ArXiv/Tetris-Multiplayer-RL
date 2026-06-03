"""DQN / Double DQN trainer for the Tetris placement bot.

This trainer keeps deployment simple by using the canonical
``common.models.TetrisPolicyNet`` as the Q-network: ``policy_logits`` are
interpreted as Q-values over the 40 placement actions, and the value head is
unused. ``--target-mode ddqn`` uses Double DQN targets; ``--target-mode dqn``
uses the classic target-network max. Checkpoints saved here load directly in
``netbot.export_onnx``.

Run from ``python/`` after the Colab setup notebook builds ``tetris_py``::

    python -m train.dqn_tetris --steps 200000 --out checkpoints/dqn.pt
    python -m netbot.export_onnx checkpoints/dqn.eval_best.pt ../model/dqn.onnx

This is intended for Colab or another training machine. Do not run long
training jobs on low-power deployment machines.
"""

from __future__ import annotations

import argparse
import copy
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

from train.rl_common import (
    LinearSchedule,
    ReplayBuffer,
    evaluate_greedy,
    greedy_action,
    masked_argmax,
)
from common.checkpoint import load_checkpoint, save_checkpoint
from common.env import TetrisPlacementEnv
from common.features import bcts_score
from common.models import TetrisPolicyNet


def shaped_reward(next_obs: dict[str, np.ndarray], lines: float, coef: float) -> float:
    """Optional BCTS shaping for sparse early DQN learning."""
    if coef == 0.0:
        return float(lines)
    board = np.asarray(next_obs["board"], dtype=np.float32).reshape(20, 10)
    return float(lines) + float(coef) * bcts_score(board, int(lines))


@torch.no_grad()
def epsilon_greedy_action(
    model: TetrisPolicyNet,
    obs: dict[str, np.ndarray],
    legal_mask: np.ndarray,
    *,
    epsilon: float,
    device: torch.device,
) -> int:
    legal = np.flatnonzero(np.asarray(legal_mask, dtype=bool))
    if legal.size == 0:
        raise RuntimeError("Cannot select from an all-false legal mask.")
    if np.random.random() < epsilon:
        return int(np.random.choice(legal))
    return greedy_action(model, obs, legal_mask, device)


def train_step(
    model: TetrisPolicyNet,
    target: TetrisPolicyNet,
    opt: torch.optim.Optimizer,
    replay: ReplayBuffer,
    *,
    batch_size: int,
    gamma: float,
    target_mode: str,
    device: torch.device,
    max_grad_norm: float,
) -> float:
    batch = replay.sample(batch_size, device)

    q_all, _ = model(batch["board"], batch["current"], batch["next"])
    q = q_all.gather(1, batch["action"].unsqueeze(1)).squeeze(1)

    with torch.no_grad():
        next_q_target, _ = target(batch["next_board"], batch["next_current"], batch["next_next"])
        if target_mode == "ddqn":
            next_q_online, _ = model(batch["next_board"], batch["next_current"], batch["next_next"])
            next_actions = masked_argmax(next_q_online, batch["next_mask"]).unsqueeze(1)
            next_q = next_q_target.gather(1, next_actions).squeeze(1)
        else:
            next_q = next_q_target.masked_fill(~batch["next_mask"], float("-inf")).max(dim=1).values

        no_next = batch["done"].bool() | (~batch["next_mask"].any(dim=1))
        target_q = batch["reward"] + gamma * (~no_next).float() * next_q

    loss = F.smooth_l1_loss(q, target_q)
    opt.zero_grad(set_to_none=True)
    loss.backward()
    nn_utils.clip_grad_norm_(model.parameters(), max_grad_norm)
    opt.step()
    return float(loss.detach().item())


def train(args: argparse.Namespace) -> None:
    device = torch.device(args.device)
    torch.manual_seed(args.seed)
    np.random.seed(args.seed)

    env = TetrisPlacementEnv(seed=args.seed)
    if args.resume and os.path.exists(args.resume):
        print(f"[dqn] resuming from {args.resume}")
        model = load_checkpoint(args.resume, device=device)
        model.train()
    else:
        model = TetrisPolicyNet().to(device)
    target = copy.deepcopy(model).to(device).eval()
    opt = torch.optim.Adam(model.parameters(), lr=args.lr, eps=1e-5)
    replay = ReplayBuffer(args.replay_size)
    eps_schedule = LinearSchedule(args.eps_start, args.eps_end, args.eps_decay_steps)

    out_path = Path(args.out)
    out_path.parent.mkdir(parents=True, exist_ok=True)

    obs, info = env.reset(seed=args.seed)
    ep_lines = 0.0
    ep_len = 0
    ep_count = 0
    recent_lines: list[float] = []
    recent_len: list[int] = []
    losses: list[float] = []
    best_eval_lines = -1e9
    t0 = time.time()

    for step in range(1, args.steps + 1):
        mask = np.asarray(info["legal_mask"], dtype=bool)
        if not mask.any():
            obs, info = env.reset()
            ep_lines = 0.0
            ep_len = 0
            mask = np.asarray(info["legal_mask"], dtype=bool)

        epsilon = eps_schedule.value(step)
        action = epsilon_greedy_action(model, obs, mask, epsilon=epsilon, device=device)
        next_obs, raw_reward, term, trunc, next_info = env.step(action)
        done = bool(term or trunc)
        next_mask = np.asarray(next_info["legal_mask"], dtype=bool)
        reward = shaped_reward(next_obs, float(raw_reward), args.shaping_coef)

        replay.add(obs, mask, action, reward, done, next_obs, next_mask)

        obs, info = next_obs, next_info
        ep_lines += float(raw_reward)
        ep_len += 1
        if done:
            ep_count += 1
            recent_lines.append(ep_lines)
            recent_len.append(ep_len)
            recent_lines = recent_lines[-50:]
            recent_len = recent_len[-50:]
            obs, info = env.reset()
            ep_lines = 0.0
            ep_len = 0

        if len(replay) >= args.warmup and step % args.train_every == 0:
            for _ in range(args.updates_per_step):
                loss = train_step(
                    model,
                    target,
                    opt,
                    replay,
                    batch_size=args.batch,
                    gamma=args.gamma,
                    target_mode=args.target_mode,
                    device=device,
                    max_grad_norm=args.max_grad_norm,
                )
                losses.append(loss)
                losses = losses[-100:]

        if step % args.target_every == 0:
            target.load_state_dict(model.state_dict())
            target.eval()

        if step % args.log_every == 0:
            sps = int(step / max(1e-6, time.time() - t0))
            avg_lines = float(np.mean(recent_lines)) if recent_lines else float("nan")
            avg_len = float(np.mean(recent_len)) if recent_len else float("nan")
            avg_loss = float(np.mean(losses)) if losses else float("nan")
            print(
                f"[dqn] step {step:>9} eps {epsilon:.3f} replay {len(replay):>7} "
                f"episodes {ep_count:>5} lines/ep {avg_lines:7.2f} len {avg_len:7.1f} "
                f"loss {avg_loss:8.4f} {sps} step/s",
                flush=True,
            )

        if args.eval_every > 0 and args.eval_episodes > 0 and step % args.eval_every == 0:
            stats = evaluate_greedy(
                model,
                episodes=args.eval_episodes,
                seed=args.eval_seed,
                device=device,
                max_pieces=args.eval_max_pieces,
            )
            print(
                f"[eval] step {step:>9} avg_lines {stats['avg_lines']:8.2f} "
                f"avg_score {stats['avg_score']:9.1f} avg_pieces {stats['avg_pieces']:8.1f}",
                flush=True,
            )
            if stats["avg_lines"] > best_eval_lines:
                best_eval_lines = stats["avg_lines"]
                save_checkpoint(
                    model,
                    out_path.with_suffix(".eval_best.pt"),
                    extra={
                        "algorithm": args.target_mode,
                        "target_mode": args.target_mode,
                        "training_steps": step,
                        "eval_avg_lines": stats["avg_lines"],
                        "eval_avg_score": stats["avg_score"],
                        "eval_avg_pieces": stats["avg_pieces"],
                    },
                )

        if args.save_every > 0 and step % args.save_every == 0:
            save_checkpoint(
                model,
                out_path,
                extra={
                    "algorithm": args.target_mode,
                    "target_mode": args.target_mode,
                    "training_steps": step,
                    "epsilon": epsilon,
                    "replay_size": len(replay),
                },
            )

    save_checkpoint(
        model,
        out_path,
        extra={
            "algorithm": args.target_mode,
            "target_mode": args.target_mode,
            "training_steps": args.steps,
            "replay_size": len(replay),
        },
    )
    print(f"[dqn] done. saved {out_path}")


def build_argparser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(description="DQN / Double DQN for Tetris placement bot")
    p.add_argument("--target-mode", choices=("ddqn", "dqn"), default="ddqn",
                   help="ddqn = online argmax + target gather; dqn = target max")
    p.add_argument("--steps", type=int, default=500_000)
    p.add_argument("--warmup", type=int, default=10_000)
    p.add_argument("--replay-size", type=int, default=200_000)
    p.add_argument("--batch", type=int, default=256)
    p.add_argument("--train-every", type=int, default=1)
    p.add_argument("--updates-per-step", type=int, default=1)
    p.add_argument("--target-every", type=int, default=2_000)
    p.add_argument("--gamma", type=float, default=0.99)
    p.add_argument("--lr", type=float, default=1e-4)
    p.add_argument("--max-grad-norm", type=float, default=10.0)
    p.add_argument("--eps-start", type=float, default=1.0)
    p.add_argument("--eps-end", type=float, default=0.05)
    p.add_argument("--eps-decay-steps", type=int, default=250_000)
    p.add_argument("--shaping-coef", type=float, default=0.02,
                   help="BCTS shaping coefficient; 0 uses raw line clears only")
    p.add_argument("--out", type=str, default="checkpoints/dqn.pt")
    p.add_argument("--resume", type=str, default="")
    p.add_argument("--save-every", type=int, default=10_000)
    p.add_argument("--log-every", type=int, default=1_000)
    p.add_argument("--eval-every", type=int, default=25_000)
    p.add_argument("--eval-episodes", type=int, default=5)
    p.add_argument("--eval-max-pieces", type=int, default=5000)
    p.add_argument("--eval-seed", type=int, default=200_000)
    p.add_argument("--seed", type=int, default=42)
    p.add_argument("--device", type=str,
                   default="cuda" if torch.cuda.is_available() else "cpu")
    return p


if __name__ == "__main__":
    train(build_argparser().parse_args())
