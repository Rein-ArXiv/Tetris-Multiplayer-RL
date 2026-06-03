"""REINFORCE / A2C / n-step Actor-Critic trainers for Tetris placement.

All modes train the canonical ``TetrisPolicyNet`` and therefore save
checkpoints that export directly to ONNX:

    python -m train.policy_gradient_tetris --algo a2c --out checkpoints/a2c.pt
    python -m netbot.export_onnx checkpoints/a2c.eval_best.pt ../model/bots/a2c.onnx

Available modes:

- ``reinforce``: episodic Monte-Carlo policy gradient with value baseline.
- ``a2c``: synchronous advantage actor-critic on fixed rollouts.
- ``nstep-ac``: same actor-critic update, usually run with shorter rollouts for
  lower-latency n-step targets.
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

from train.rl_common import bcts_shaped_reward, evaluate_greedy, obs_to_batch
from common import BOARD_COLS, BOARD_ROWS, NUM_PIECE_TYPES
from common.checkpoint import load_checkpoint, save_checkpoint
from common.env import TetrisPlacementEnv
from common.models import TetrisPolicyNet, masked_log_softmax


def _masked_logp_entropy(
    logits: torch.Tensor,
    mask: torch.Tensor,
) -> tuple[torch.Tensor, torch.Tensor]:
    logp = masked_log_softmax(logits, mask)
    probs = logp.exp()
    entropy = -torch.where(mask, probs * logp, torch.zeros_like(probs)).sum(dim=1)
    return logp, entropy


@torch.no_grad()
def sample_action(
    model: TetrisPolicyNet,
    obs: dict[str, np.ndarray],
    mask_np: np.ndarray,
    *,
    device: torch.device,
    temperature: float,
) -> tuple[int, float]:
    mask = torch.as_tensor(mask_np, dtype=torch.bool, device=device).unsqueeze(0)
    batch = obs_to_batch(obs, device)
    logits, value = model(batch["board"], batch["current"], batch["next"])
    logits = logits / max(float(temperature), 1e-6)
    logp, _entropy = _masked_logp_entropy(logits, mask)
    probs = logp.exp()
    action = torch.multinomial(probs, 1).squeeze(1)
    return int(action.item()), float(value.squeeze(0).item())


def to_tensors(data: dict[str, list], device: torch.device) -> dict[str, torch.Tensor]:
    return {
        "board": torch.as_tensor(np.stack(data["board"]), dtype=torch.float32, device=device),
        "current": torch.as_tensor(np.stack(data["current"]), dtype=torch.float32, device=device),
        "next": torch.as_tensor(np.stack(data["next"]), dtype=torch.float32, device=device),
        "mask": torch.as_tensor(np.stack(data["mask"]), dtype=torch.bool, device=device),
        "action": torch.as_tensor(np.asarray(data["action"], dtype=np.int64), dtype=torch.long, device=device),
        "return": torch.as_tensor(np.asarray(data["return"], dtype=np.float32), dtype=torch.float32, device=device),
        "advantage": torch.as_tensor(
            np.asarray(data["advantage"], dtype=np.float32),
            dtype=torch.float32,
            device=device,
        ),
    }


def update_policy(
    model: TetrisPolicyNet,
    opt: torch.optim.Optimizer,
    data: dict[str, list],
    args: argparse.Namespace,
    *,
    device: torch.device,
) -> dict[str, float]:
    batch = to_tensors(data, device)
    n = int(batch["action"].shape[0])
    idx_np = np.arange(n)

    pg_total = 0.0
    v_total = 0.0
    ent_total = 0.0
    updates = 0
    for _epoch in range(args.epochs):
        np.random.shuffle(idx_np)
        for start in range(0, n, args.batch):
            idx = torch.as_tensor(idx_np[start:start + args.batch], dtype=torch.long, device=device)
            logits, value = model(batch["board"][idx], batch["current"][idx], batch["next"][idx])
            logp, entropy = _masked_logp_entropy(logits, batch["mask"][idx])
            action_logp = logp.gather(1, batch["action"][idx].unsqueeze(1)).squeeze(1)

            adv = batch["advantage"][idx]
            if args.normalize_advantage and adv.numel() > 1:
                adv = (adv - adv.mean()) / (adv.std() + 1e-8)

            pg_loss = -(action_logp * adv.detach()).mean()
            value_loss = F.mse_loss(value, batch["return"][idx])
            entropy_loss = entropy.mean()
            loss = pg_loss + args.value_coef * value_loss - args.entropy_coef * entropy_loss

            opt.zero_grad(set_to_none=True)
            loss.backward()
            nn_utils.clip_grad_norm_(model.parameters(), args.max_grad_norm)
            opt.step()

            pg_total += float(pg_loss.detach().item())
            v_total += float(value_loss.detach().item())
            ent_total += float(entropy_loss.detach().item())
            updates += 1

    denom = max(1, updates)
    return {
        "pg": pg_total / denom,
        "value": v_total / denom,
        "entropy": ent_total / denom,
    }


def append_transition(
    data: dict[str, list],
    obs: dict[str, np.ndarray],
    mask: np.ndarray,
    action: int,
) -> None:
    data["board"].append(np.asarray(obs["board"], dtype=np.float32))
    data["current"].append(np.asarray(obs["current"], dtype=np.float32))
    data["next"].append(np.asarray(obs["next"], dtype=np.float32))
    data["mask"].append(np.asarray(mask, dtype=np.bool_))
    data["action"].append(int(action))


def discounted_returns(rewards: list[float], gamma: float, bootstrap: float = 0.0) -> list[float]:
    out = [0.0] * len(rewards)
    ret = float(bootstrap)
    for i in reversed(range(len(rewards))):
        ret = float(rewards[i]) + gamma * ret
        out[i] = ret
    return out


def collect_reinforce_episode(
    model: TetrisPolicyNet,
    env: TetrisPlacementEnv,
    args: argparse.Namespace,
    *,
    device: torch.device,
) -> tuple[dict[str, list], float, int]:
    obs, info = env.reset()
    data: dict[str, list] = {
        "board": [],
        "current": [],
        "next": [],
        "mask": [],
        "action": [],
        "return": [],
        "advantage": [],
    }
    rewards: list[float] = []
    values: list[float] = []
    raw_lines = 0.0

    for _piece in range(args.max_pieces):
        mask = np.asarray(info["legal_mask"], dtype=bool)
        if not mask.any():
            break
        action, value = sample_action(model, obs, mask, device=device, temperature=args.temperature)
        append_transition(data, obs, mask, action)
        next_obs, raw_reward, term, trunc, info = env.step(action)
        rewards.append(bcts_shaped_reward(next_obs, float(raw_reward), args.shaping_coef))
        values.append(value)
        raw_lines += float(raw_reward)
        obs = next_obs
        if term or trunc:
            break

    returns = discounted_returns(rewards, args.gamma)
    data["return"] = returns
    data["advantage"] = [ret - val for ret, val in zip(returns, values, strict=True)]
    return data, raw_lines, len(rewards)


def collect_a2c_rollout(
    model: TetrisPolicyNet,
    env: TetrisPlacementEnv,
    obs: dict[str, np.ndarray],
    info: dict,
    args: argparse.Namespace,
    *,
    device: torch.device,
) -> tuple[dict[str, list], dict[str, np.ndarray], dict, list[float], list[int]]:
    data: dict[str, list] = {
        "board": [],
        "current": [],
        "next": [],
        "mask": [],
        "action": [],
        "return": [],
        "advantage": [],
    }
    rewards: list[float] = []
    values: list[float] = []
    episode_lines: list[float] = []
    episode_lengths: list[int] = []
    ep_lines = 0.0
    ep_len = 0

    for _ in range(args.rollout):
        mask = np.asarray(info["legal_mask"], dtype=bool)
        if not mask.any():
            obs, info = env.reset()
            ep_lines = 0.0
            ep_len = 0
            mask = np.asarray(info["legal_mask"], dtype=bool)

        action, value = sample_action(model, obs, mask, device=device, temperature=args.temperature)
        append_transition(data, obs, mask, action)
        next_obs, raw_reward, term, trunc, next_info = env.step(action)
        rewards.append(bcts_shaped_reward(next_obs, float(raw_reward), args.shaping_coef))
        values.append(value)
        ep_lines += float(raw_reward)
        ep_len += 1
        obs, info = next_obs, next_info

        if term or trunc or ep_len >= args.max_pieces:
            episode_lines.append(ep_lines)
            episode_lengths.append(ep_len)
            obs, info = env.reset()
            ep_lines = 0.0
            ep_len = 0

    bootstrap = 0.0
    if np.asarray(info["legal_mask"], dtype=bool).any():
        with torch.no_grad():
            batch = obs_to_batch(obs, device)
            _logits, v = model(batch["board"], batch["current"], batch["next"])
            bootstrap = float(v.squeeze(0).item())

    returns = discounted_returns(rewards, args.gamma, bootstrap)
    data["return"] = returns
    data["advantage"] = [ret - val for ret, val in zip(returns, values, strict=True)]
    return data, obs, info, episode_lines, episode_lengths


def maybe_eval_and_save(
    model: TetrisPolicyNet,
    args: argparse.Namespace,
    out_path: Path,
    step: int,
    best_eval_lines: float,
    *,
    device: torch.device,
) -> float:
    if args.eval_every <= 0 or args.eval_episodes <= 0:
        return best_eval_lines

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
        save_checkpoint(
            model,
            out_path.with_suffix(".eval_best.pt"),
            extra={
                "algorithm": args.algo,
                "training_steps": step,
                "eval_avg_lines": stats["avg_lines"],
                "eval_avg_score": stats["avg_score"],
                "eval_avg_pieces": stats["avg_pieces"],
            },
        )
        return stats["avg_lines"]
    return best_eval_lines


def train(args: argparse.Namespace) -> None:
    device = torch.device(args.device)
    torch.manual_seed(args.seed)
    np.random.seed(args.seed)

    if args.resume and os.path.exists(args.resume):
        print(f"[pg] resuming from {args.resume}")
        model = load_checkpoint(args.resume, device=device)
        model.train()
    else:
        model = TetrisPolicyNet().to(device)
    opt = torch.optim.Adam(model.parameters(), lr=args.lr, eps=1e-5)
    out_path = Path(args.out)
    out_path.parent.mkdir(parents=True, exist_ok=True)

    env = TetrisPlacementEnv(seed=args.seed)
    obs, info = env.reset(seed=args.seed)
    step = 0
    update = 0
    best_eval_lines = -1e9
    next_eval_step = args.eval_every if args.eval_every > 0 else args.steps + 1
    recent_lines: list[float] = []
    recent_len: list[int] = []
    t0 = time.time()

    while step < args.steps:
        model.eval()
        if args.algo == "reinforce":
            data, lines, pieces = collect_reinforce_episode(model, env, args, device=device)
            recent_lines.append(lines)
            recent_len.append(pieces)
            batch_steps = pieces
        else:
            data, obs, info, lines_list, len_list = collect_a2c_rollout(
                model,
                env,
                obs,
                info,
                args,
                device=device,
            )
            recent_lines.extend(lines_list)
            recent_len.extend(len_list)
            batch_steps = len(data["action"])

        if batch_steps == 0:
            obs, info = env.reset()
            continue

        model.train()
        stats = update_policy(model, opt, data, args, device=device)
        step += batch_steps
        update += 1
        recent_lines = recent_lines[-50:]
        recent_len = recent_len[-50:]

        if args.log_every > 0 and update % args.log_every == 0:
            sps = int(step / max(1e-6, time.time() - t0))
            print(
                f"[{args.algo}] upd {update:>5} step {step:>9} "
                f"lines/ep {np.mean(recent_lines) if recent_lines else float('nan'):7.2f} "
                f"len {np.mean(recent_len) if recent_len else float('nan'):7.1f} "
                f"pg {stats['pg']:+.4f} v {stats['value']:.4f} ent {stats['entropy']:.4f} "
                f"{sps} step/s",
                flush=True,
            )

        if args.eval_every > 0 and step >= next_eval_step:
            best_eval_lines = maybe_eval_and_save(
                model,
                args,
                out_path,
                step,
                best_eval_lines,
                device=device,
            )
            while next_eval_step <= step:
                next_eval_step += args.eval_every

        if args.save_every > 0 and update % args.save_every == 0:
            save_checkpoint(
                model,
                out_path,
                extra={
                    "algorithm": args.algo,
                    "training_steps": step,
                    "update": update,
                },
            )

    save_checkpoint(
        model,
        out_path,
        extra={
            "algorithm": args.algo,
            "training_steps": step,
            "update": update,
        },
    )
    print(f"[{args.algo}] done. saved {out_path}")


def build_argparser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(description="REINFORCE / A2C / n-step AC for Tetris")
    p.add_argument("--algo", choices=("reinforce", "a2c", "nstep-ac"), default="a2c")
    p.add_argument("--steps", type=int, default=500_000)
    p.add_argument("--rollout", type=int, default=1024,
                   help="steps per update for a2c/nstep-ac")
    p.add_argument("--epochs", type=int, default=1)
    p.add_argument("--batch", type=int, default=256)
    p.add_argument("--lr", type=float, default=3e-4)
    p.add_argument("--gamma", type=float, default=0.99)
    p.add_argument("--entropy-coef", type=float, default=0.01)
    p.add_argument("--value-coef", type=float, default=0.5)
    p.add_argument("--max-grad-norm", type=float, default=0.5)
    p.add_argument("--temperature", type=float, default=1.0)
    p.add_argument("--normalize-advantage", action="store_true", default=True)
    p.add_argument("--no-normalize-advantage", action="store_false", dest="normalize_advantage")
    p.add_argument("--shaping-coef", type=float, default=0.02)
    p.add_argument("--max-pieces", type=int, default=5000)
    p.add_argument("--out", type=str, default="checkpoints/a2c.pt")
    p.add_argument("--resume", type=str, default="")
    p.add_argument("--save-every", type=int, default=10)
    p.add_argument("--log-every", type=int, default=1)
    p.add_argument("--eval-every", type=int, default=25_000)
    p.add_argument("--eval-episodes", type=int, default=5)
    p.add_argument("--eval-max-pieces", type=int, default=5000)
    p.add_argument("--eval-seed", type=int, default=400_000)
    p.add_argument("--seed", type=int, default=42)
    p.add_argument("--device", type=str,
                   default="cuda" if torch.cuda.is_available() else "cpu")
    return p


if __name__ == "__main__":
    train(build_argparser().parse_args())
