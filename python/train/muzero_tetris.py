"""MuZero-style trainer for the Tetris placement bot.

This is a compact MuZero-style baseline for Colab experimentation:

- representation: observation -> latent state
- dynamics: latent state + placement action -> next latent state + reward
- prediction: latent state -> policy logits + value
- self-play: root MCTS over legal placement actions

The native MuZero checkpoint is *not* deployable by the current C++ ONNX bot.
After training, this script distills the MCTS policy targets into the canonical
``TetrisPolicyNet`` and saves ``*.policy.pt``. Export that distilled policy:

    python -m train.muzero_tetris --episodes 200 --out checkpoints/muzero.pt
    python -m netbot.export_onnx checkpoints/muzero.policy.pt ../model/muzero.onnx
"""

from __future__ import annotations

import argparse
import math
import os
import sys
import time
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any

import numpy as np
import torch
import torch.nn as nn
import torch.nn.functional as F
import torch.nn.utils as nn_utils

_PY_ROOT = Path(__file__).resolve().parent.parent
if str(_PY_ROOT) not in sys.path:
    sys.path.insert(0, str(_PY_ROOT))

from train.rl_common import ReplayBuffer, masked_softmax_np, obs_to_batch
from common import BOARD_COLS, BOARD_ROWS, NUM_PIECE_TYPES, NUM_PLACEMENTS
from common.checkpoint import save_checkpoint
from common.env import TetrisPlacementEnv
from common.models import TetrisPolicyNet


class MuZeroNet(nn.Module):
    """Small MuZero-style network for placement-level Tetris."""

    def __init__(
        self,
        *,
        hidden: int = 256,
        latent: int = 256,
        n_actions: int = NUM_PLACEMENTS,
    ) -> None:
        super().__init__()
        self.hidden = hidden
        self.latent = latent
        self.n_actions = n_actions

        self.trunk = nn.Sequential(
            nn.Conv2d(1, 32, kernel_size=3, padding=1),
            nn.ReLU(inplace=True),
            nn.Conv2d(32, 64, kernel_size=3, padding=1),
            nn.ReLU(inplace=True),
            nn.Conv2d(64, 64, kernel_size=3, padding=1),
            nn.ReLU(inplace=True),
        )
        self.representation = nn.Sequential(
            nn.Linear(64 * BOARD_ROWS * BOARD_COLS + 2 * NUM_PIECE_TYPES, hidden),
            nn.ReLU(inplace=True),
            nn.Linear(hidden, latent),
            nn.Tanh(),
        )
        self.prediction_body = nn.Sequential(
            nn.Linear(latent, hidden),
            nn.ReLU(inplace=True),
        )
        self.policy_head = nn.Linear(hidden, n_actions)
        self.value_head = nn.Linear(hidden, 1)

        self.dynamics_body = nn.Sequential(
            nn.Linear(latent + n_actions, hidden),
            nn.ReLU(inplace=True),
            nn.Linear(hidden, hidden),
            nn.ReLU(inplace=True),
        )
        self.next_latent_head = nn.Sequential(
            nn.Linear(hidden, latent),
            nn.Tanh(),
        )
        self.reward_head = nn.Linear(hidden, 1)

    def represent(
        self,
        board: torch.Tensor,
        current: torch.Tensor,
        next_piece: torch.Tensor,
    ) -> torch.Tensor:
        h = self.trunk(board)
        h = h.flatten(1)
        h = torch.cat([h, current, next_piece], dim=-1)
        return self.representation(h)

    def predict(self, latent: torch.Tensor) -> tuple[torch.Tensor, torch.Tensor]:
        h = self.prediction_body(latent)
        return self.policy_head(h), self.value_head(h).squeeze(-1)

    def dynamics(self, latent: torch.Tensor, action: torch.Tensor) -> tuple[torch.Tensor, torch.Tensor]:
        action_one_hot = F.one_hot(action.long(), num_classes=self.n_actions).float()
        h = self.dynamics_body(torch.cat([latent, action_one_hot], dim=-1))
        return self.next_latent_head(h), self.reward_head(h).squeeze(-1)


@dataclass
class SearchNode:
    prior: float
    latent: torch.Tensor | None = None
    reward: float = 0.0
    visit_count: int = 0
    value_sum: float = 0.0
    children: dict[int, "SearchNode"] = field(default_factory=dict)

    @property
    def value(self) -> float:
        if self.visit_count == 0:
            return 0.0
        return self.value_sum / float(self.visit_count)


@torch.no_grad()
def expand_node(
    net: MuZeroNet,
    node: SearchNode,
    *,
    mask: np.ndarray | None,
    device: torch.device,
) -> float:
    if node.latent is None:
        raise RuntimeError("Cannot expand a node without latent state.")

    logits, value = net.predict(node.latent)
    if mask is None:
        legal = np.arange(net.n_actions, dtype=np.int64)
        prior = torch.softmax(logits.squeeze(0), dim=-1).detach().cpu().numpy().astype(np.float32)
    else:
        mask = np.asarray(mask, dtype=bool)
        legal = np.flatnonzero(mask).astype(np.int64)
        prior = masked_softmax_np(logits.squeeze(0), mask)

    if legal.size == 0:
        return float(value.squeeze(0).item())

    latent_batch = node.latent.repeat(int(legal.size), 1)
    action_batch = torch.as_tensor(legal, dtype=torch.long, device=device)
    next_latent, reward = net.dynamics(latent_batch, action_batch)

    for i, action in enumerate(legal):
        node.children[int(action)] = SearchNode(
            prior=float(prior[int(action)]),
            latent=next_latent[i:i + 1].detach(),
            reward=float(reward[i].item()),
        )
    return float(value.squeeze(0).item())


def add_root_noise(root: SearchNode, alpha: float, frac: float) -> None:
    if alpha <= 0.0 or frac <= 0.0 or not root.children:
        return
    actions = list(root.children.keys())
    noise = np.random.dirichlet([alpha] * len(actions))
    for action, n in zip(actions, noise, strict=True):
        child = root.children[action]
        child.prior = (1.0 - frac) * child.prior + frac * float(n)


def select_child(parent: SearchNode, *, pb_c_base: float, pb_c_init: float, gamma: float) -> SearchNode:
    best_score = -float("inf")
    best_child: SearchNode | None = None
    parent_visits = max(parent.visit_count, 1)
    pb_c = math.log((parent_visits + pb_c_base + 1.0) / pb_c_base) + pb_c_init
    pb_c *= math.sqrt(parent_visits)

    for child in parent.children.values():
        prior_score = pb_c * child.prior / float(child.visit_count + 1)
        value_score = child.reward + gamma * child.value
        score = value_score + prior_score
        if score > best_score:
            best_score = score
            best_child = child

    if best_child is None:
        raise RuntimeError("select_child called on an unexpanded node.")
    return best_child


def backup(path: list[SearchNode], value: float, gamma: float) -> None:
    for node in reversed(path):
        node.value_sum += value
        node.visit_count += 1
        value = node.reward + gamma * value


@torch.no_grad()
def run_mcts(
    net: MuZeroNet,
    obs: dict[str, np.ndarray],
    legal_mask: np.ndarray,
    args: argparse.Namespace,
    *,
    device: torch.device,
) -> tuple[int, np.ndarray]:
    net.eval()
    mask = np.asarray(legal_mask, dtype=bool)
    legal = np.flatnonzero(mask)
    if legal.size == 0:
        raise RuntimeError("Cannot run MCTS with no legal root actions.")

    batch = obs_to_batch(obs, device)
    root_latent = net.represent(batch["board"], batch["current"], batch["next"])
    root = SearchNode(prior=1.0, latent=root_latent.detach())
    expand_node(net, root, mask=mask, device=device)
    add_root_noise(root, args.root_dirichlet_alpha, args.root_exploration_fraction)

    for _ in range(args.mcts_simulations):
        node = root
        path = [node]
        while node.children:
            node = select_child(
                node,
                pb_c_base=args.pb_c_base,
                pb_c_init=args.pb_c_init,
                gamma=args.gamma,
            )
            path.append(node)
        value = expand_node(net, node, mask=None, device=device)
        backup(path, value, args.gamma)

    visits = np.zeros(NUM_PLACEMENTS, dtype=np.float32)
    for action, child in root.children.items():
        visits[action] = float(child.visit_count)
    if visits.sum() <= 0.0:
        for action, child in root.children.items():
            visits[action] = float(child.prior)
    visits *= mask.astype(np.float32)
    if visits.sum() <= 0.0:
        visits[legal] = 1.0

    if args.action_temperature <= 1e-6:
        policy = np.zeros(NUM_PLACEMENTS, dtype=np.float32)
        policy[int(legal[np.argmax(visits[legal])])] = 1.0
    else:
        scaled = np.power(visits, 1.0 / args.action_temperature)
        scaled *= mask.astype(np.float32)
        policy = scaled / max(float(scaled.sum()), 1e-12)

    action = int(np.random.choice(np.arange(NUM_PLACEMENTS), p=policy))
    return action, policy.astype(np.float32)


def self_play_episode(
    net: MuZeroNet,
    replay: ReplayBuffer,
    args: argparse.Namespace,
    *,
    episode: int,
    device: torch.device,
) -> tuple[float, int]:
    env = TetrisPlacementEnv(seed=args.seed + episode)
    obs, info = env.reset(seed=args.seed + episode)
    trajectory: list[dict[str, Any]] = []
    lines = 0.0

    for _piece in range(args.max_pieces):
        mask = np.asarray(info["legal_mask"], dtype=bool)
        if not mask.any():
            break
        action, policy = run_mcts(net, obs, mask, args, device=device)
        next_obs, reward, term, trunc, next_info = env.step(action)
        done = bool(term or trunc)
        trajectory.append(
            {
                "obs": obs,
                "mask": mask,
                "action": action,
                "reward": float(reward),
                "done": done,
                "next_obs": next_obs,
                "next_mask": np.asarray(next_info["legal_mask"], dtype=bool),
                "policy": policy,
            }
        )
        lines += float(reward)
        obs, info = next_obs, next_info
        if done:
            break

    value = 0.0
    returns = [0.0] * len(trajectory)
    for i in reversed(range(len(trajectory))):
        value = trajectory[i]["reward"] + args.gamma * value
        returns[i] = value

    for item, target_value in zip(trajectory, returns, strict=True):
        replay.add(
            item["obs"],
            item["mask"],
            item["action"],
            item["reward"] / args.reward_scale,
            item["done"],
            item["next_obs"],
            item["next_mask"],
            policy_target=item["policy"],
            value_target=target_value / args.value_scale,
        )

    return lines, len(trajectory)


def policy_loss(logits: torch.Tensor, mask: torch.Tensor, target: torch.Tensor) -> torch.Tensor:
    target = target / target.sum(dim=1, keepdim=True).clamp_min(1e-6)
    logp = F.log_softmax(logits.masked_fill(~mask, -1e9), dim=-1)
    return -(target * logp).sum(dim=1).mean()


def train_muzero_step(
    net: MuZeroNet,
    opt: torch.optim.Optimizer,
    replay: ReplayBuffer,
    args: argparse.Namespace,
    *,
    device: torch.device,
) -> dict[str, float]:
    batch = replay.sample(args.batch, device)
    latent = net.represent(batch["board"], batch["current"], batch["next"])
    logits, value = net.predict(latent)
    pi = policy_loss(logits, batch["mask"], batch["policy_target"])
    v = F.mse_loss(value, batch["value_target"])

    pred_next_latent, pred_reward = net.dynamics(latent, batch["action"])
    reward_loss = F.mse_loss(pred_reward, batch["reward"])

    with torch.no_grad():
        target_next_latent = net.represent(batch["next_board"], batch["next_current"], batch["next_next"])
    consistency = F.mse_loss(
        F.normalize(pred_next_latent, dim=-1),
        F.normalize(target_next_latent, dim=-1),
    )

    loss = pi + args.value_loss_coef * v + args.reward_loss_coef * reward_loss
    loss = loss + args.consistency_loss_coef * consistency
    opt.zero_grad(set_to_none=True)
    loss.backward()
    nn_utils.clip_grad_norm_(net.parameters(), args.max_grad_norm)
    opt.step()

    return {
        "loss": float(loss.detach().item()),
        "policy": float(pi.detach().item()),
        "value": float(v.detach().item()),
        "reward": float(reward_loss.detach().item()),
        "consistency": float(consistency.detach().item()),
    }


def save_muzero_checkpoint(net: MuZeroNet, path: Path, args: argparse.Namespace, *, episodes: int) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    torch.save(
        {
            "state_dict": net.state_dict(),
            "__meta__": {
                "class": "MuZeroNet",
                "algorithm": "muzero_style",
                "episodes": episodes,
                "hidden": net.hidden,
                "latent": net.latent,
                "n_actions": net.n_actions,
                "value_scale": args.value_scale,
                "reward_scale": args.reward_scale,
            },
        },
        str(path),
    )


def load_muzero_checkpoint(path: str | Path, device: torch.device) -> MuZeroNet:
    payload = torch.load(str(path), map_location=device, weights_only=True)
    meta = payload.get("__meta__", {})
    net = MuZeroNet(
        hidden=int(meta.get("hidden", 256)),
        latent=int(meta.get("latent", 256)),
        n_actions=int(meta.get("n_actions", NUM_PLACEMENTS)),
    ).to(device)
    net.load_state_dict(payload["state_dict"])
    return net


def distill_policy(
    replay: ReplayBuffer,
    args: argparse.Namespace,
    *,
    device: torch.device,
    out_path: Path,
) -> None:
    if len(replay) == 0 or args.distill_steps <= 0:
        return

    policy = TetrisPolicyNet().to(device)
    opt = torch.optim.Adam(policy.parameters(), lr=args.distill_lr, eps=1e-5)
    losses: list[float] = []
    policy.train()
    for step in range(1, args.distill_steps + 1):
        batch = replay.sample(args.distill_batch, device)
        logits, value = policy(batch["board"], batch["current"], batch["next"])
        pi = policy_loss(logits, batch["mask"], batch["policy_target"])
        v = F.mse_loss(value, batch["value_target"])
        loss = pi + args.distill_value_loss_coef * v
        opt.zero_grad(set_to_none=True)
        loss.backward()
        nn_utils.clip_grad_norm_(policy.parameters(), args.max_grad_norm)
        opt.step()
        losses.append(float(loss.detach().item()))

        if args.distill_log_every > 0 and step % args.distill_log_every == 0:
            print(
                f"[distill] step {step:>6}/{args.distill_steps} "
                f"loss {np.mean(losses[-100:]):.4f}",
                flush=True,
            )

    save_checkpoint(
        policy,
        out_path,
        extra={
            "algorithm": "muzero_distilled_policy",
            "distill_steps": args.distill_steps,
            "replay_size": len(replay),
            "value_scale": args.value_scale,
        },
    )
    print(f"[distill] saved deployable policy checkpoint {out_path}")


def train(args: argparse.Namespace) -> None:
    device = torch.device(args.device)
    torch.manual_seed(args.seed)
    np.random.seed(args.seed)

    if args.resume and os.path.exists(args.resume):
        print(f"[muzero] resuming from {args.resume}")
        net = load_muzero_checkpoint(args.resume, device)
    else:
        net = MuZeroNet(hidden=args.hidden, latent=args.latent).to(device)
    opt = torch.optim.Adam(net.parameters(), lr=args.lr, eps=1e-5)
    replay = ReplayBuffer(args.replay_size)

    out_path = Path(args.out)
    policy_out = Path(args.policy_out) if args.policy_out else out_path.with_suffix(".policy.pt")
    lines_window: list[float] = []
    len_window: list[int] = []
    loss_window: list[float] = []
    t0 = time.time()

    for episode in range(1, args.episodes + 1):
        lines, pieces = self_play_episode(net, replay, args, episode=episode, device=device)
        lines_window.append(lines)
        len_window.append(pieces)
        lines_window = lines_window[-50:]
        len_window = len_window[-50:]

        if len(replay) >= args.warmup:
            net.train()
            for _ in range(args.train_steps_per_episode):
                stats = train_muzero_step(net, opt, replay, args, device=device)
                loss_window.append(stats["loss"])
                loss_window = loss_window[-100:]

        if args.log_every > 0 and episode % args.log_every == 0:
            elapsed = max(time.time() - t0, 1e-6)
            print(
                f"[muzero] ep {episode:>6}/{args.episodes} replay {len(replay):>7} "
                f"lines/ep {np.mean(lines_window):7.2f} pieces {np.mean(len_window):7.1f} "
                f"loss {np.mean(loss_window) if loss_window else float('nan'):.4f} "
                f"{episode / elapsed:.2f} ep/s",
                flush=True,
            )

        if args.save_every > 0 and episode % args.save_every == 0:
            save_muzero_checkpoint(net, out_path, args, episodes=episode)

    save_muzero_checkpoint(net, out_path, args, episodes=args.episodes)
    print(f"[muzero] saved native MuZero checkpoint {out_path}")
    distill_policy(replay, args, device=device, out_path=policy_out)


def build_argparser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(description="MuZero-style trainer for Tetris placement bot")
    p.add_argument("--episodes", type=int, default=500)
    p.add_argument("--max-pieces", type=int, default=1000)
    p.add_argument("--replay-size", type=int, default=200_000)
    p.add_argument("--warmup", type=int, default=2_000)
    p.add_argument("--batch", type=int, default=256)
    p.add_argument("--train-steps-per-episode", type=int, default=32)
    p.add_argument("--mcts-simulations", type=int, default=32)
    p.add_argument("--action-temperature", type=float, default=1.0)
    p.add_argument("--root-dirichlet-alpha", type=float, default=0.30)
    p.add_argument("--root-exploration-fraction", type=float, default=0.25)
    p.add_argument("--pb-c-base", type=float, default=19652.0)
    p.add_argument("--pb-c-init", type=float, default=1.25)
    p.add_argument("--gamma", type=float, default=0.99)
    p.add_argument("--value-scale", type=float, default=20.0)
    p.add_argument("--reward-scale", type=float, default=20.0,
                   help="scale rewards into the same unit as value targets")
    p.add_argument("--value-loss-coef", type=float, default=0.25)
    p.add_argument("--reward-loss-coef", type=float, default=1.0)
    p.add_argument("--consistency-loss-coef", type=float, default=0.1)
    p.add_argument("--hidden", type=int, default=256)
    p.add_argument("--latent", type=int, default=256)
    p.add_argument("--lr", type=float, default=1e-4)
    p.add_argument("--max-grad-norm", type=float, default=5.0)
    p.add_argument("--out", type=str, default="checkpoints/muzero.pt")
    p.add_argument("--resume", type=str, default="")
    p.add_argument("--policy-out", type=str, default="")
    p.add_argument("--save-every", type=int, default=25)
    p.add_argument("--log-every", type=int, default=5)
    p.add_argument("--distill-steps", type=int, default=2_000)
    p.add_argument("--distill-batch", type=int, default=256)
    p.add_argument("--distill-lr", type=float, default=3e-4)
    p.add_argument("--distill-value-loss-coef", type=float, default=0.25)
    p.add_argument("--distill-log-every", type=int, default=200)
    p.add_argument("--seed", type=int, default=42)
    p.add_argument("--device", type=str,
                   default="cuda" if torch.cuda.is_available() else "cpu")
    return p


if __name__ == "__main__":
    train(build_argparser().parse_args())
