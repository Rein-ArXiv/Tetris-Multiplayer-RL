"""Hand-rolled PPO trainer for the Tetris placement bot.

Trains the repo's canonical ``common.models.TetrisPolicyNet`` directly on
``common.env.TetrisPlacementEnv``. Training on *this* network (instead of a
framework's own policy class) is deliberate: the checkpoint it writes loads
straight into ``netbot`` and exports cleanly via ``netbot.export_onnx`` to the
``model/bots/*.onnx`` files the C++ in-game bot roster reads. No weight
transfer needed.

Baseline run (from the ``python/`` directory)::

    python -m train.ppo_tetris --steps 1000000 --out checkpoints/run.pt

Resume / fine-tune::

    python -m train.ppo_tetris --resume checkpoints/run.pt --steps 500000

Then export for the C++/netbot side::

    python -m netbot.export_onnx checkpoints/run.pt ../model/bots/run.onnx

This is the baseline loop for character-bot bring-up: single synchronous env,
legal-action-masked PPO, periodic greedy evaluation, and checkpoint files that
export cleanly to ONNX. It is not the final high-performance trainer; tune
``--shaping-coef`` / network / rollout size and add vectorized envs later.
"""

from __future__ import annotations

import argparse
import sys
import time
from pathlib import Path

import numpy as np
import torch
import torch.nn as nn

# python/ 을 import 경로에 넣는다. `python train/ppo_tetris.py` 로 직접 실행할 때와
# `python -m train.ppo_tetris` 로 모듈 실행할 때 모두 common/, sim/, netbot/ 을
# 찾을 수 있게 하려는 것이다. 모듈 실행이면 이미 잡혀 있으므로 건너뛴다.
_PY_ROOT = Path(__file__).resolve().parent.parent
if str(_PY_ROOT) not in sys.path:
    sys.path.insert(0, str(_PY_ROOT))

from common import BOARD_COLS, BOARD_ROWS  # noqa: E402
from common.checkpoint import load_checkpoint, save_checkpoint  # noqa: E402
from common.env import TetrisPlacementEnv  # noqa: E402
from common.env_versus import TetrisVersusEnv  # noqa: E402
from common.models import TetrisPolicyNet, masked_log_softmax, masked_entropy  # noqa: E402
from common.ppo_loss import clipped_policy_loss
from common.returns import gae_targets, normalize_advantages  # noqa: E402
from common.model_contract import positive_size  # noqa: E402


# --- 보상 shaping (선택) ---
# 환경 보상에 보드 벌점을 매 전이마다 추가하는 선택적 학습 목적이다.
# 보상이 드문 구간에 신호를 주지만 작은 계수도 원래 최적 정책의 보존을 보장하지 않는다.
# 잠재함수의 차이가 아니라 방문한 보드마다 반복되는 벌점이다.
# 원래 줄/공격/승패 지표는 shaping을 더하지 않은 평가에서 따로 비교한다.
_W_HOLE = 0.03
_W_HEIGHT = 0.005
_W_BUMP = 0.003


def board_features(board: np.ndarray) -> tuple[int, int, int]:
    """Return holes, aggregate height and bumpiness for schema-sized binary occupancy."""
    array = np.asarray(board)
    if array.shape not in ((BOARD_ROWS, BOARD_COLS), (1, BOARD_ROWS, BOARD_COLS)):
        raise ValueError("board shape does not match the observation schema")
    if array.dtype.kind not in "buif" or not np.isfinite(array).all() or not np.isin(array, [0, 1]).all():
        raise ValueError("board must contain finite binary occupancy")
    b = array.reshape(BOARD_ROWS, BOARD_COLS) > 0
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
    coef = float(coef)
    if not np.isfinite(coef):
        raise ValueError("shaping coefficient must be finite")
    if coef == 0.0:
        return 0.0
    holes, agg_height, bumpiness = board_features(board)
    penalty = _W_HOLE * holes + _W_HEIGHT * agg_height + _W_BUMP * bumpiness
    reward = -coef * penalty
    if not np.isfinite(reward):
        raise ValueError("shaping reward is not finite")
    return reward


# --- 평가 ---
def _logp_entropy(logits: torch.Tensor, mask: torch.Tensor):
    """Masked log-probs + per-row entropy, NaN-safe for illegal actions."""
    logp = masked_log_softmax(logits, mask)          # (B, A); -inf on illegal
    probs = logp.exp()                               # 0 on illegal
    entropy = masked_entropy(logp, mask)             # (B,); finite backward too
    return logp, probs, entropy


def to_batch(obs: dict, device) -> dict:
    return {
        "board": torch.as_tensor(obs["board"], dtype=torch.float32, device=device).unsqueeze(0),
        "current": torch.as_tensor(obs["current"], dtype=torch.float32, device=device).unsqueeze(0),
        "next": torch.as_tensor(obs["next"], dtype=torch.float32, device=device).unsqueeze(0),
    }


def make_env(kind: str, seed: int):
    """학습/평가에 쓸 환경을 만든다.

    single — 혼자 두는 판. 보상은 지운 줄 수뿐이다.
    versus — 상대와 garbage를 주고받는 2-보드 판. 관측 형태가 single과 같아서
             같은 TetrisPolicyNet을 그대로 쓸 수 있다.
    """
    if kind == "versus":
        return TetrisVersusEnv(seed=seed)
    return TetrisPlacementEnv(seed=seed)


@torch.no_grad()
def evaluate_policy(
    model: TetrisPolicyNet,
    *,
    episodes: int,
    seed: int,
    device: torch.device,
    max_pieces: int,
    env_kind: str = "single",
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
            "avg_reward": float("nan"),
            "episodes": 0.0,
        }

    positive_size(max_pieces, "max_pieces")
    was_training = model.training
    env = None
    lines_total = score_total = pieces_total = reward_total = 0.0
    ended_count = truncated_count = cutoff_count = 0
    try:
        model.eval()
        env = make_env(env_kind, seed)
        for ep in range(episodes):
            obs, info = env.reset(seed=seed + ep)
            pieces = 0
            episode_reward = 0.0
            term = trunc = False
            while pieces < max_pieces:
                mask_np = info["legal_mask"]
                if not mask_np.any():
                    raise RuntimeError("live evaluation observation has no legal action")
                batch = to_batch(obs, device)
                mask = torch.as_tensor(mask_np, dtype=torch.bool, device=device).unsqueeze(0)
                logits, _ = model(batch["board"], batch["current"], batch["next"])
                action = int(masked_log_softmax(logits, mask).argmax(-1).item())
                obs, reward, term, trunc, info = env.step(action)
                episode_reward += float(reward)
                pieces += 1
                if term or trunc:
                    break
            ended_count += int(term)
            truncated_count += int(trunc)
            cutoff_count += int(pieces == max_pieces and not (term or trunc))
            lines_total += float(info["lines"])
            score_total += float(info["score"])
            pieces_total += pieces
            reward_total += episode_reward
    finally:
        model.train(was_training)
        if env is not None:
            env.close()
    denom = float(episodes)
    return {
        "avg_lines": lines_total / denom,
        "avg_score": score_total / denom,
        "avg_pieces": pieces_total / denom,
        "avg_reward": reward_total / denom,
        "terminated_episodes": ended_count,
        "truncated_episodes": truncated_count,
        "budget_cutoffs": cutoff_count,
        "episodes": denom,
    }


# --- 학습 루프 ---
def validate_train_args(args):
    for name in ('steps', 'rollout', 'epochs', 'minibatch', 'eval_max_pieces'):
        positive_size(getattr(args, name), name)
    for name in ('save_every', 'eval_every', 'eval_episodes'):
        value = getattr(args, name)
        if isinstance(value, bool) or not isinstance(value, int) or value < 0:
            raise ValueError(name + ' must be a nonnegative integer')
    for name in ('gamma', 'lam'):
        value = getattr(args, name)
        if isinstance(value, bool) or not np.isfinite(value) or not 0 <= value <= 1:
            raise ValueError(name + ' must be finite and in [0,1]')
    for name in ('lr', 'clip', 'max_grad_norm'):
        value = getattr(args, name)
        if isinstance(value, bool) or not np.isfinite(value) or value <= 0:
            raise ValueError(name + ' must be finite and positive')
    for name in ('ent_coef', 'vf_coef'):
        value = getattr(args, name)
        if isinstance(value, bool) or not np.isfinite(value) or value < 0:
            raise ValueError(name + ' must be finite and nonnegative')
    if not np.isfinite(args.shaping_coef):
        raise ValueError('shaping_coef must be finite')


def train(args: argparse.Namespace) -> None:
    validate_train_args(args)
    device = torch.device(args.device)
    torch.manual_seed(args.seed)
    np.random.seed(args.seed)

    if args.resume:
        print(f"[ppo] warm start weights from {args.resume}; optimizer/RNG/environment restart")
        model = load_checkpoint(args.resume, device=device)
        model.train()
    else:
        model = TetrisPolicyNet().to(device)
    opt = torch.optim.Adam(model.parameters(), lr=args.lr, eps=1e-5)

    out_path = Path(args.out)
    out_path.parent.mkdir(parents=True, exist_ok=True)

    env = make_env(args.env, args.seed)
    print(f"[ppo] env={args.env}")
    obs, info = env.reset(seed=args.seed)
    ep_ret, ep_len, ep_lines = 0.0, 0, 0
    ep_returns: list[float] = []
    ep_lengths: list[int] = []
    ep_lineclears: list[int] = []
    best_mean = -1e9
    best_eval_score = -float("inf")
    selection_metric = "avg_reward" if args.env == "versus" else "avg_lines"

    global_step = 0
    update = 0
    t_start = time.time()

    while global_step < args.steps:
        T = min(args.rollout, args.steps - global_step)
        # rollout 버퍼. 환경을 하나만 동기로 돌리므로 배열 하나면 충분하다.
        boards = torch.zeros(T, 1, BOARD_ROWS, BOARD_COLS, device=device)
        currents = torch.zeros(T, model.n_piece_types, device=device)
        nexts = torch.zeros(T, model.n_piece_types, device=device)
        masks = torch.zeros(T, model.n_placements, dtype=torch.bool, device=device)
        actions = torch.zeros(T, dtype=torch.long, device=device)
        logps = torch.zeros(T, device=device)
        values = torch.zeros(T, device=device)
        rewards = torch.zeros(T, device=device)
        terminated = torch.zeros(T, dtype=torch.bool, device=device)
        boundaries = torch.zeros(T, dtype=torch.bool, device=device)
        next_values = torch.zeros(T, device=device)

        for t in range(T):
            mask_np = info["legal_mask"]
            if not mask_np.any():
                raise RuntimeError("live rollout observation has no legal action; check environment termination")

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
            terminated[t] = bool(term)
            boundaries[t] = bool(term or trunc)
            # Bootstrap from the actual transition endpoint, never from reset().
            if not term:
                with torch.no_grad():
                    endpoint = to_batch(next_obs, device)
                    _, next_value = model(endpoint["board"], endpoint["current"], endpoint["next"])
                    next_values[t] = next_value[0]

            ep_ret += total_r
            ep_len += 1
            ep_lines = int(next_info["lines"])
            global_step += 1

            obs, info = next_obs, next_info
            if term or trunc:
                ep_returns.append(ep_ret)
                ep_lengths.append(ep_len)
                ep_lineclears.append(ep_lines)
                ep_ret, ep_len, ep_lines = 0.0, 0, 0
                obs, info = env.reset()

        advantages, returns = gae_targets(
            rewards, values, next_values, torch.full_like(rewards, args.gamma),
            terminated, boundaries, args.lam,
        )
        adv = normalize_advantages(advantages)

        # Reuse this rollout for bounded epochs; clipping is an objective term,
        # not a hard guarantee on policy movement or gradient magnitude.
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

                pg_loss, approximate_kl, clip_fraction = clipped_policy_loss(
                    new_logp, logps[jt], adv[jt], args.clip,
                )

                v_loss = 0.5 * (value - returns[jt]).pow(2).mean()
                ent_loss = entropy.mean()

                loss = pg_loss + args.vf_coef * v_loss - args.ent_coef * ent_loss
                if not torch.isfinite(loss):
                    raise ValueError("PPO loss is not finite")
                opt.zero_grad(set_to_none=True)
                loss.backward()
                nn.utils.clip_grad_norm_(model.parameters(), args.max_grad_norm, error_if_nonfinite=True)
                opt.step()

        update += 1

        # 진행 상황 출력
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
            f"kl_last {approximate_kl.item():.4f} clip_last {clip_fraction.item():.3f}  "
            f"{sps} step/s",
            flush=True,
        )

        # greedy policy로 평가 (탐색 없이 최선 수만)
        eval_stats: dict[str, float] | None = None
        if args.eval_episodes > 0 and args.eval_every > 0 and update % args.eval_every == 0:
            eval_stats = evaluate_policy(
                model,
                episodes=args.eval_episodes,
                seed=args.eval_seed,
                device=device,
                env_kind=args.env,
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
            if eval_stats[selection_metric] > best_eval_score:
                best_eval_score = eval_stats[selection_metric]
                save_checkpoint(
                    model,
                    out_path.with_suffix(".eval_best.pt"),
                    extra={
                        "training_steps": global_step,
                        "update": update,
                        "eval_avg_lines": eval_stats["avg_lines"],
                        "eval_avg_reward": eval_stats["avg_reward"],
                        "selection_metric": selection_metric,
                        "eval_avg_score": eval_stats["avg_score"],
                        "eval_avg_pieces": eval_stats["avg_pieces"],
                        "eval_episodes": int(eval_stats["episodes"]),
                        "eval_seed": args.eval_seed,
                    },
                )

        # 체크포인트 저장
        if args.save_every > 0 and update % args.save_every == 0:
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
    env.close()
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
    p.add_argument("--resume", type=str, default="", help="weights-only warm start; new optimizer, RNG and environment")
    p.add_argument("--save-every", type=int, default=10, help="updates between saves")
    p.add_argument("--eval-every", type=int, default=10,
                   help="PPO updates between greedy eval runs (0 = disable periodic eval)")
    p.add_argument("--eval-episodes", type=int, default=5,
                   help="number of fixed-seed episodes per eval (0 = disable eval)")
    p.add_argument("--eval-max-pieces", type=int, default=5000,
                   help="cap per eval episode so strong policies cannot run forever")
    p.add_argument("--eval-seed", type=int, default=100_000,
                   help="base seed for fixed-seed evaluation episodes")
    p.add_argument("--env", type=str, default="single",
                   choices=["single", "versus"],
                   help="single = 1인 판, versus = garbage 교환 2-보드 판 "
                        "(versus 는 gymnasium 필요)")
    p.add_argument("--seed", type=int, default=42)
    p.add_argument("--device", type=str,
                   default="cuda" if torch.cuda.is_available() else "cpu")
    return p


if __name__ == "__main__":
    train(build_argparser().parse_args())
