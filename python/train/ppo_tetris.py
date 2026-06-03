"""Hand-rolled PPO trainer for the Tetris placement bot.

Trains the repo's canonical ``common.models.TetrisPolicyNet`` directly on
``common.env.TetrisPlacementEnv``. Training on *this* network (instead of a
framework's own policy class) is deliberate: the checkpoint it writes loads
straight into ``netbot`` and exports cleanly via ``netbot.export_onnx`` to the
``model/policy.onnx`` the C++ in-game bot reads. No weight transfer needed.

Baseline run (from the ``python/`` directory)::

    python -m train.ppo_tetris --steps 1000000 --out checkpoints/run.pt

Resume / fine-tune::

    python -m train.ppo_tetris --resume checkpoints/run.pt --steps 500000

Then export for the C++/netbot side::

    python -m netbot.export_onnx checkpoints/run.pt ../model/policy.onnx

This is the baseline loop for character-bot bring-up: single synchronous env,
legal-action-masked PPO, periodic greedy evaluation, and checkpoint files that
export cleanly to ONNX. It is not the final high-performance trainer; tune
``--shaping-coef`` / network / rollout size and add vectorized envs later.
"""

from __future__ import annotations

import argparse
import os
import sys
import time
from pathlib import Path

import numpy as np
import torch
import torch.nn as nn

# Allow running as a plain script (`python train/ppo_tetris.py`) as well as a
# module — make sure the python/ root (which holds common/, sim/, netbot/) is
# importable.
_PY_ROOT = Path(__file__).resolve().parent.parent
if str(_PY_ROOT) not in sys.path:
    sys.path.insert(0, str(_PY_ROOT))

from common import BOARD_COLS, BOARD_ROWS  # noqa: E402
from common.checkpoint import load_checkpoint, save_checkpoint  # noqa: E402
from common.env import TetrisPlacementEnv  # noqa: E402
from common.models import TetrisPolicyNet, masked_log_softmax  # noqa: E402


# ─── Reward shaping (optional, tunable) ──────────────────────────────────────
# The env reward is just lines-cleared (0..4) per placement — very sparse early
# in training (a fresh policy may go thousands of placements before its first
# line). These dense, interpretable board features give a gradient before any
# line clears: prefer low stacks, few holes, flat surfaces. Scaled by
# --shaping-coef (set 0 for pure lines-cleared reward).
_W_HOLE = 0.03
_W_HEIGHT = 0.005
_W_BUMP = 0.003


def board_features(board: np.ndarray) -> tuple[int, int, int]:
    """Return (holes, aggregate_height, bumpiness) for a (1,20,10) occupancy."""
    b = board.reshape(BOARD_ROWS, BOARD_COLS) > 0.5
    holes = 0
    heights = np.zeros(BOARD_COLS, dtype=np.int64)
    for c in range(BOARD_COLS):
        col = b[:, c]
        filled = np.flatnonzero(col)
        if filled.size == 0:
            continue
        top = int(filled[0])                 # 0 = top row
        heights[c] = BOARD_ROWS - top
        holes += int(np.count_nonzero(~col[top:]))
    agg_height = int(heights.sum())
    bumpiness = int(np.abs(np.diff(heights)).sum())
    return holes, agg_height, bumpiness


def shaping_reward(board: np.ndarray, coef: float) -> float:
    if coef == 0.0:
        return 0.0
    holes, agg_height, bumpiness = board_features(board)
    penalty = _W_HOLE * holes + _W_HEIGHT * agg_height + _W_BUMP * bumpiness
    return -coef * penalty


# ─── Policy evaluation helpers ───────────────────────────────────────────────
def _logp_entropy(logits: torch.Tensor, mask: torch.Tensor):
    """Masked log-probs + per-row entropy, NaN-safe for illegal actions."""
    logp = masked_log_softmax(logits, mask)          # (B, A); -inf on illegal
    probs = logp.exp()                               # 0 on illegal
    ent_terms = torch.where(mask, probs * logp, torch.zeros_like(probs))
    entropy = -ent_terms.sum(-1)                     # (B,)
    return logp, probs, entropy


def to_batch(obs: dict, device) -> dict:
    return {
        "board": torch.as_tensor(obs["board"], dtype=torch.float32, device=device).unsqueeze(0),
        "current": torch.as_tensor(obs["current"], dtype=torch.float32, device=device).unsqueeze(0),
        "next": torch.as_tensor(obs["next"], dtype=torch.float32, device=device).unsqueeze(0),
    }


@torch.no_grad()
def evaluate_policy(
    model: TetrisPolicyNet,
    *,
    episodes: int,
    seed: int,
    device: torch.device,
    max_pieces: int,
) -> dict[str, float]:
    """Run greedy placement evaluation on fixed seeds.

    The training reward may include shaping. Evaluation intentionally reports
    raw gameplay metrics only so different reward-shaping experiments remain
    comparable for character balancing.
    """
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
    lines_total = 0.0
    score_total = 0.0
    pieces_total = 0.0

    for ep in range(episodes):
        obs, info = env.reset(seed=seed + ep)
        lines = 0.0
        score = 0.0
        pieces = 0

        while pieces < max_pieces:
            mask_np = info["legal_mask"]
            if not mask_np.any():
                break

            batch = to_batch(obs, device)
            mask = torch.as_tensor(mask_np, dtype=torch.bool, device=device)
            logits, _value = model(batch["board"], batch["current"], batch["next"])
            masked_logits = logits.squeeze(0).masked_fill(~mask, float("-inf"))
            if torch.isinf(masked_logits).all():
                break

            action = int(torch.argmax(masked_logits).item())
            obs, reward, term, trunc, info = env.step(action)
            lines += float(reward)
            score = float(info.get("score", score))
            pieces += 1

            if term or trunc:
                break

        lines_total += lines
        score_total += score
        pieces_total += float(pieces)

    if was_training:
        model.train()

    denom = float(episodes)
    return {
        "avg_lines": lines_total / denom,
        "avg_score": score_total / denom,
        "avg_pieces": pieces_total / denom,
        "episodes": denom,
    }


# ─── Training ────────────────────────────────────────────────────────────────
def train(args: argparse.Namespace) -> None:
    device = torch.device(args.device)
    torch.manual_seed(args.seed)
    np.random.seed(args.seed)

    env = TetrisPlacementEnv(seed=args.seed)

    if args.resume and os.path.exists(args.resume):
        print(f"[ppo] resuming from {args.resume}")
        model = load_checkpoint(args.resume, device=device)
        model.train()
    else:
        model = TetrisPolicyNet().to(device)
    opt = torch.optim.Adam(model.parameters(), lr=args.lr, eps=1e-5)

    out_path = Path(args.out)
    out_path.parent.mkdir(parents=True, exist_ok=True)

    obs, info = env.reset(seed=args.seed)
    ep_ret, ep_len, ep_lines = 0.0, 0, 0
    ep_returns: list[float] = []
    ep_lengths: list[int] = []
    ep_lineclears: list[int] = []
    best_mean = -1e9
    best_eval_lines = -1e9

    T = args.rollout
    global_step = 0
    update = 0
    t_start = time.time()

    while global_step < args.steps:
        # --- Rollout buffer (one synchronous env) ---------------------------
        boards = torch.zeros(T, 1, BOARD_ROWS, BOARD_COLS, device=device)
        currents = torch.zeros(T, 7, device=device)
        nexts = torch.zeros(T, 7, device=device)
        masks = torch.zeros(T, model.n_placements, dtype=torch.bool, device=device)
        actions = torch.zeros(T, dtype=torch.long, device=device)
        logps = torch.zeros(T, device=device)
        values = torch.zeros(T, device=device)
        rewards = torch.zeros(T, device=device)
        dones = torch.zeros(T, device=device)

        for t in range(T):
            mask_np = info["legal_mask"]
            if not mask_np.any():
                # Defensive: a no-legal-action state should only happen at game
                # over (already handled by the reset after a terminal step). If
                # we ever see one here, reset and fill this slot with the fresh
                # transition rather than leaving a zero/all-illegal buffer slot
                # (which would produce NaN logits in the PPO update).
                obs, info = env.reset()
                ep_ret, ep_len, ep_lines = 0.0, 0, 0
                mask_np = info["legal_mask"]

            batch = to_batch(obs, device)
            mask = torch.as_tensor(mask_np, dtype=torch.bool, device=device).unsqueeze(0)
            with torch.no_grad():
                logits, value = model(batch["board"], batch["current"], batch["next"])
                logp_row, probs, _ = _logp_entropy(logits, mask)
                action = torch.multinomial(probs, 1).squeeze(-1)        # legal-only
                logp = logp_row.gather(-1, action.unsqueeze(-1)).squeeze(-1)

            a = int(action.item())
            next_obs, reward, term, trunc, next_info = env.step(a)
            shaped = shaping_reward(next_obs["board"], args.shaping_coef)
            total_r = float(reward) + shaped

            boards[t] = batch["board"][0]
            currents[t] = batch["current"][0]
            nexts[t] = batch["next"][0]
            masks[t] = mask[0]
            actions[t] = action[0]
            logps[t] = logp[0]
            values[t] = value[0]
            rewards[t] = total_r
            dones[t] = 1.0 if (term or trunc) else 0.0

            ep_ret += total_r
            ep_len += 1
            ep_lines += int(reward)           # reward == lines cleared this placement
            global_step += 1

            obs, info = next_obs, next_info
            if term or trunc:
                ep_returns.append(ep_ret)
                ep_lengths.append(ep_len)
                ep_lineclears.append(ep_lines)
                ep_ret, ep_len, ep_lines = 0.0, 0, 0
                obs, info = env.reset()

        # --- Bootstrap value of the state after the rollout -----------------
        with torch.no_grad():
            if info["legal_mask"].any():
                b = to_batch(obs, device)
                _, last_value = model(b["board"], b["current"], b["next"])
                last_value = last_value[0]
            else:
                last_value = torch.zeros((), device=device)

        # --- GAE(λ) advantages + returns ------------------------------------
        advantages = torch.zeros(T, device=device)
        lastgae = torch.zeros((), device=device)
        for t in reversed(range(T)):
            nonterminal = 1.0 - dones[t]
            nextval = last_value if t == T - 1 else values[t + 1]
            delta = rewards[t] + args.gamma * nextval * nonterminal - values[t]
            lastgae = delta + args.gamma * args.lam * nonterminal * lastgae
            advantages[t] = lastgae
        returns = advantages + values
        adv = (advantages - advantages.mean()) / (advantages.std() + 1e-8)

        # --- PPO update (K epochs over minibatches) -------------------------
        idx = np.arange(T)
        mb = args.minibatch
        pg_loss = v_loss = ent_loss = torch.zeros((), device=device)
        for _ in range(args.epochs):
            np.random.shuffle(idx)
            for start in range(0, T, mb):
                j = idx[start:start + mb]
                jt = torch.as_tensor(j, dtype=torch.long, device=device)

                logits, value = model(boards[jt], currents[jt], nexts[jt])
                logp_row, _, entropy = _logp_entropy(logits, masks[jt])
                new_logp = logp_row.gather(-1, actions[jt].unsqueeze(-1)).squeeze(-1)

                ratio = (new_logp - logps[jt]).exp()
                mb_adv = adv[jt]
                pg1 = -mb_adv * ratio
                pg2 = -mb_adv * torch.clamp(ratio, 1 - args.clip, 1 + args.clip)
                pg_loss = torch.max(pg1, pg2).mean()

                v_loss = 0.5 * (value - returns[jt]).pow(2).mean()
                ent_loss = entropy.mean()

                loss = pg_loss + args.vf_coef * v_loss - args.ent_coef * ent_loss
                opt.zero_grad()
                loss.backward()
                nn.utils.clip_grad_norm_(model.parameters(), args.max_grad_norm)
                opt.step()

        update += 1

        # --- Logging --------------------------------------------------------
        if ep_returns:
            window = 50
            mret = float(np.mean(ep_returns[-window:]))
            mlen = float(np.mean(ep_lengths[-window:]))
            mlines = float(np.mean(ep_lineclears[-window:]))
        else:
            mret = mlen = mlines = float("nan")
        sps = int(global_step / max(1e-6, time.time() - t_start))
        print(
            f"[ppo] upd {update:>5} step {global_step:>9}  "
            f"ep_ret {mret:8.3f}  ep_len {mlen:7.1f}  lines/ep {mlines:7.2f}  "
            f"pg {pg_loss.item():+.3f} v {v_loss.item():.3f} ent {ent_loss.item():.3f}  "
            f"{sps} step/s",
            flush=True,
        )

        # --- Greedy evaluation ---------------------------------------------
        eval_stats: dict[str, float] | None = None
        if args.eval_episodes > 0 and args.eval_every > 0 and update % args.eval_every == 0:
            eval_stats = evaluate_policy(
                model,
                episodes=args.eval_episodes,
                seed=args.eval_seed,
                device=device,
                max_pieces=args.eval_max_pieces,
            )
            print(
                f"[eval] upd {update:>5} step {global_step:>9}  "
                f"avg_lines {eval_stats['avg_lines']:8.2f}  "
                f"avg_score {eval_stats['avg_score']:9.1f}  "
                f"avg_pieces {eval_stats['avg_pieces']:8.1f}  "
                f"episodes {int(eval_stats['episodes'])}",
                flush=True,
            )
            if eval_stats["avg_lines"] > best_eval_lines:
                best_eval_lines = eval_stats["avg_lines"]
                save_checkpoint(
                    model,
                    out_path.with_suffix(".eval_best.pt"),
                    extra={
                        "training_steps": global_step,
                        "update": update,
                        "eval_avg_lines": eval_stats["avg_lines"],
                        "eval_avg_score": eval_stats["avg_score"],
                        "eval_avg_pieces": eval_stats["avg_pieces"],
                        "eval_episodes": int(eval_stats["episodes"]),
                        "eval_seed": args.eval_seed,
                    },
                )

        # --- Checkpointing --------------------------------------------------
        if update % args.save_every == 0:
            save_checkpoint(model, out_path, extra={"training_steps": global_step,
                                                    "update": update})
            if ep_returns and mret > best_mean:
                best_mean = mret
                save_checkpoint(model, out_path.with_suffix(".best.pt"),
                                extra={"training_steps": global_step, "mean_return": mret})

    save_checkpoint(model, out_path, extra={"training_steps": global_step, "update": update})
    if args.eval_episodes > 0:
        final_eval = evaluate_policy(
            model,
            episodes=args.eval_episodes,
            seed=args.eval_seed + 1_000_000,
            device=device,
            max_pieces=args.eval_max_pieces,
        )
        print(
            f"[eval] final step {global_step:>9}  "
            f"avg_lines {final_eval['avg_lines']:8.2f}  "
            f"avg_score {final_eval['avg_score']:9.1f}  "
            f"avg_pieces {final_eval['avg_pieces']:8.1f}  "
            f"episodes {int(final_eval['episodes'])}",
            flush=True,
        )
    print(f"[ppo] done. saved {out_path} ({global_step} steps)")


def build_argparser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(description="Hand-rolled PPO for Tetris placement bot")
    p.add_argument("--steps", type=int, default=1_000_000, help="total env steps")
    p.add_argument("--rollout", type=int, default=2048, help="steps per PPO update")
    p.add_argument("--epochs", type=int, default=4, help="PPO epochs per update")
    p.add_argument("--minibatch", type=int, default=256)
    p.add_argument("--lr", type=float, default=3e-4)
    p.add_argument("--gamma", type=float, default=0.99)
    p.add_argument("--lam", type=float, default=0.95, help="GAE lambda")
    p.add_argument("--clip", type=float, default=0.2, help="PPO clip epsilon")
    p.add_argument("--ent-coef", type=float, default=0.01)
    p.add_argument("--vf-coef", type=float, default=0.5)
    p.add_argument("--max-grad-norm", type=float, default=0.5)
    p.add_argument("--shaping-coef", type=float, default=0.5,
                   help="weight on dense board-feature shaping (0 = pure lines)")
    p.add_argument("--out", type=str, default="checkpoints/run.pt")
    p.add_argument("--resume", type=str, default="", help="checkpoint to resume from")
    p.add_argument("--save-every", type=int, default=10, help="updates between saves")
    p.add_argument("--eval-every", type=int, default=10,
                   help="PPO updates between greedy eval runs (0 = disable periodic eval)")
    p.add_argument("--eval-episodes", type=int, default=5,
                   help="number of fixed-seed episodes per eval (0 = disable eval)")
    p.add_argument("--eval-max-pieces", type=int, default=5000,
                   help="cap per eval episode so strong policies cannot run forever")
    p.add_argument("--eval-seed", type=int, default=100_000,
                   help="base seed for fixed-seed evaluation episodes")
    p.add_argument("--seed", type=int, default=42)
    p.add_argument("--device", type=str,
                   default="cuda" if torch.cuda.is_available() else "cpu")
    return p


if __name__ == "__main__":
    train(build_argparser().parse_args())
