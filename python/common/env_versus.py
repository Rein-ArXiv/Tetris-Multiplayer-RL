"""Two-board competitive (versus) Tetris environment.

This is the garbage-trading counterpart to ``common.env.TetrisPlacementEnv``.
The learning agent controls board A; an *opponent* controls board B. Line
clears send garbage to the other board, exactly mirroring the C++ game's
combat wiring (``src/main.cpp``: take the ``attack_lines_sent()`` delta after a
placement and route it to the other board's ``add_pending_garbage()``; the
garbage is injected at the receiving board's next lock — ``SimGame::LockBlock``).

Design goals:

* **Drop-in for existing single-agent trainers.** The observation is identical
  to ``TetrisPlacementEnv`` (the agent's own ``board``/``current``/``next``), so
  ``common.models.TetrisPolicyNet`` and the PPO/DQN/A2C/CEM loops train against
  it unchanged. The competitive pressure arrives *through the board* (received
  garbage raises the agent's stack) and *through the reward* (attack sent +
  win/loss bonus). Extra competitive signals live in ``info`` for wrappers that
  want them.
* **Self-play ready.** Pass any ``opponent`` — a scripted heuristic
  (``GreedyBCTSOpponent``, the default), random legal play
  (``RandomLegalOpponent``), or a snapshot of the current policy via
  ``PolicyOpponent`` — to train against a frozen copy of yourself.

Action / observation contract (agent side)::

    action_space      = Discrete(40)                      # encode_action(col, rot)
    observation_space = Dict(board, current, next)        # same as single-player
    info["legal_mask"]        = bool (40,)
    info["incoming_garbage"]  = int   # queued on the agent's board
    info["agent_attack"]      = int   # lines the agent sent this step
    info["opp_attack"]        = int   # lines the opponent sent this step
    info["opp_alive"]         = bool

Reward per step = ``lines_cleared + attack_weight * attack_sent``; on the
terminal step ``+win_bonus`` if only the opponent topped out, ``-loss_penalty``
if the agent topped out (including a simultaneous top-out). Treating a mutual
top-out as a loss prevents the learning agent from exploiting suicidal attacks.
"""

from __future__ import annotations

import random
from typing import Any, Callable, Optional

import numpy as np

try:
    import gymnasium as gym
    from gymnasium import spaces
    _HAS_GYM = True
except ImportError:  # pragma: no cover - gymnasium is optional at import time
    gym = None  # type: ignore
    spaces = None  # type: ignore
    _HAS_GYM = False

from . import BOARD_COLS, BOARD_ROWS, NUM_PIECE_TYPES, NUM_PLACEMENTS
from .action_mask import decode_action, encode_action, legal_mask
from .features import bcts_score
from .obs import build_observation


def _terminal_bonus(
    a_dead: bool, b_dead: bool, win_bonus: float, loss_penalty: float
) -> float:
    """Terminal reward from the learning agent's perspective."""
    if b_dead and not a_dead:
        return float(win_bonus)
    if a_dead:
        return -float(loss_penalty)
    return 0.0


# ─── Opponents ───────────────────────────────────────────────────────────────
class VersusOpponent:
    """Decides one placement for a board. Return an encoded action (0..39) or
    ``None`` if the board has no legal move (treated as a pass)."""

    def reset(self) -> None:  # noqa: D401 - optional hook
        """Called on env reset. Override to reset per-episode state."""

    def act(self, sim: Any) -> Optional[int]:
        raise NotImplementedError


class RandomLegalOpponent(VersusOpponent):
    """Picks a uniformly random legal placement. Seeded for reproducibility."""

    def __init__(self, seed: int | None = None) -> None:
        self._rng = random.Random(seed)

    def act(self, sim: Any) -> Optional[int]:
        placements = sim.legal_placements()
        if not placements:
            return None
        p = self._rng.choice(placements)
        return encode_action(int(p.col), int(p.rot))


class GreedyBCTSOpponent(VersusOpponent):
    """One-ply Dellacherie/BCTS greedy: clone the board, try every legal
    placement, keep the one with the best post-placement BCTS score. A solid,
    dependency-light sparring partner (the same evaluator CBMPI improves on)."""

    def __init__(self, line_weight: float = 1.0) -> None:
        self._line_weight = line_weight

    def act(self, sim: Any) -> Optional[int]:
        placements = sim.legal_placements()
        if not placements:
            return None
        best_action = None
        best_score = -float("inf")
        for p in placements:
            child = sim.clone()
            cleared = int(child.apply_placement(int(p.col), int(p.rot)))
            if cleared < 0:
                continue
            board = np.asarray(child.grid(), dtype=np.float32)
            score = self._line_weight * float(cleared) + bcts_score(board, cleared)
            if score > best_score:
                best_score = score
                best_action = encode_action(int(p.col), int(p.rot))
        return best_action


class PolicyOpponent(VersusOpponent):
    """Wraps a callable ``policy_fn(obs_dict, legal_mask_np) -> action`` so a
    trained (or snapshot) policy can be the opponent for self-play. The env
    builds the opponent's own observation from its board before calling."""

    def __init__(self, policy_fn: Callable[[dict[str, np.ndarray], np.ndarray], int]) -> None:
        self._policy_fn = policy_fn

    def act(self, sim: Any) -> Optional[int]:
        if not sim.legal_placements():
            return None
        obs = {k: v.numpy() for k, v in build_observation(sim).items()}
        mask = legal_mask(sim).numpy()
        if not mask.any():
            return None
        return int(self._policy_fn(obs, mask))


# ─── Environment ─────────────────────────────────────────────────────────────
class TetrisVersusEnv(gym.Env if _HAS_GYM else object):  # type: ignore[misc]
    """Single-agent view of a 2-board garbage-trading match."""

    metadata = {"render_modes": []}

    def __init__(
        self,
        seed: int | None = None,
        opponent: VersusOpponent | None = None,
        *,
        attack_weight: float = 0.5,
        win_bonus: float = 10.0,
        loss_penalty: float = 10.0,
        max_pieces: int = 2000,
        opponent_seed: int | None = None,
    ) -> None:
        if not _HAS_GYM:
            raise ImportError(
                "gymnasium is required for TetrisVersusEnv. "
                "Install it with `pip install gymnasium`."
            )
        from sim import SimGame  # noqa: PLC0415 - lazy so common/ imports without the native module

        self._SimGame = SimGame
        self._seed = seed if seed is not None else 0
        # Independent piece sequences by default (typical for versus play).
        self._opp_seed = opponent_seed if opponent_seed is not None else self._seed + 1
        self._opponent = opponent if opponent is not None else GreedyBCTSOpponent()

        self.attack_weight = float(attack_weight)
        self.win_bonus = float(win_bonus)
        self.loss_penalty = float(loss_penalty)
        self.max_pieces = int(max_pieces)

        self.simA: Any = None  # agent board
        self.simB: Any = None  # opponent board
        self._last_attack_a = 0
        self._last_attack_b = 0
        self._pieces = 0

        self.action_space = spaces.Discrete(NUM_PLACEMENTS)
        self.observation_space = spaces.Dict(
            {
                "board": spaces.Box(0.0, 1.0, (1, BOARD_ROWS, BOARD_COLS), np.float32),
                "current": spaces.Box(0.0, 1.0, (NUM_PIECE_TYPES,), np.float32),
                "next": spaces.Box(0.0, 1.0, (NUM_PIECE_TYPES,), np.float32),
            }
        )

    # ---- Gym API ---------------------------------------------------------
    def reset(
        self,
        *,
        seed: int | None = None,
        options: dict[str, Any] | None = None,
    ) -> tuple[dict[str, np.ndarray], dict[str, Any]]:
        if seed is not None:
            self._seed = seed
            self._opp_seed = seed + 1
        self.simA = self._SimGame(self._seed)
        self.simB = self._SimGame(self._opp_seed)
        self._last_attack_a = 0
        self._last_attack_b = 0
        self._pieces = 0
        self._opponent.reset()
        return self._observation(self.simA), self._info(0, 0)

    def step(
        self, action: int
    ) -> tuple[dict[str, np.ndarray], float, bool, bool, dict[str, Any]]:
        assert self.simA is not None, "Call reset() before step()"

        col, rot = decode_action(int(action))
        cleared = self.simA.apply_placement(col, rot)

        if cleared < 0:
            # Illegal placement — don't advance the agent board. A correct,
            # mask-respecting policy never hits this; tolerate it defensively.
            return (
                self._observation(self.simA),
                0.0,
                self.simA.game_over(),
                False,
                self._info(0, 0),
            )

        # Route the agent's attack to the opponent board.
        agent_attack = self.simA.attack_lines_sent() - self._last_attack_a
        self._last_attack_a = self.simA.attack_lines_sent()
        if agent_attack > 0:
            self.simB.add_pending_garbage(agent_attack)

        # Opponent plays one piece (if still alive), routing its attack back.
        opp_attack = 0
        if not self.simB.game_over():
            opp_action = self._opponent.act(self.simB)
            if opp_action is not None:
                bc, br = decode_action(int(opp_action))
                if self.simB.apply_placement(bc, br) >= 0:
                    opp_attack = self.simB.attack_lines_sent() - self._last_attack_b
                    self._last_attack_b = self.simB.attack_lines_sent()
                    if opp_attack > 0:
                        self.simA.add_pending_garbage(opp_attack)

        a_dead = self.simA.game_over()
        b_dead = self.simB.game_over()

        reward = float(cleared) + self.attack_weight * float(agent_attack)
        terminated = a_dead or b_dead
        if terminated:
            reward += _terminal_bonus(
                a_dead, b_dead, self.win_bonus, self.loss_penalty
            )
            # Mutual top-out is an agent loss: do not reward suicidal attacks.

        self._pieces += 1
        truncated = self._pieces >= self.max_pieces
        return (
            self._observation(self.simA),
            reward,
            terminated,
            truncated,
            self._info(agent_attack, opp_attack),
        )

    # ---- Internals -------------------------------------------------------
    def _observation(self, sim: Any) -> dict[str, np.ndarray]:
        return {k: v.numpy() for k, v in build_observation(sim).items()}

    def _info(self, agent_attack: int, opp_attack: int) -> dict[str, Any]:
        assert self.simA is not None and self.simB is not None
        return {
            "legal_mask": legal_mask(self.simA).numpy(),
            "score": self.simA.score(),
            "state_hash": self.simA.state_hash(),
            "incoming_garbage": self.simA.pending_garbage(),
            "agent_attack": int(agent_attack),
            "opp_attack": int(opp_attack),
            "opp_alive": not self.simB.game_over(),
            "agent_lines": self.simA.total_lines_cleared(),
            "opp_lines": self.simB.total_lines_cleared(),
        }
