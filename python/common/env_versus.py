"""Two-board competitive (versus) Tetris environment.

This is the garbage-trading counterpart to ``common.env.TetrisPlacementEnv``.
The learning agent controls board A; an *opponent* controls board B. Line
clears produce an ``attack_lines_sent()`` delta, queued through the other
board's ``add_pending_garbage()`` and injected at its next lock. These are the
same native combat primitives used by the game. This environment's ordered
A-then-B placement schedule is a turn-based approximation of real-time play.

Design goals:

* **Shared observation schema.** The agent's own ``board``/``current``/``next``
  match ``TetrisPlacementEnv`` and ``common.models.TetrisPolicyNet``. Trainers
  still need to select the environment and handle reward/termination semantics.
  Received garbage changes the board; attack and win/loss terms change reward.
  Extra competitive signals live in ``info`` for wrappers that need them.
* **Self-play ready.** Pass any ``opponent`` — a scripted heuristic
  (``GreedyBCTSOpponent``, the default), random legal play
  (``RandomLegalOpponent``), or a snapshot of the current policy via
  ``PolicyOpponent`` — to train against a frozen copy of yourself.

Action / observation contract (agent side)::

    action_space      = Discrete(NUM_PLACEMENTS)                      # encode_action(col, rot)
    observation_space = Dict(board, current, next)        # same as single-player
    info["legal_mask"]        = bool (NUM_PLACEMENTS,)
    info["incoming_garbage"]  = int   # queued on the agent's board
    info["agent_attack"]      = int   # lines the agent sent this step
    info["opp_attack"]        = int   # lines the opponent sent this step
    info["opp_alive"]         = bool

Reward per step = ``lines_cleared + attack_weight * attack_sent``; on the
terminal step ``+win_bonus`` if only the opponent topped out, ``-loss_penalty``
if the agent topped out (including a simultaneous top-out). Treating a mutual
top-out as a loss penalizes mutual failure; it does not prove that all
suicidal attack strategies have negative total return.
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
from .gym_contract import optional_seed, action_index, positive_limit, draw_seed
from .versus_reward import finite_coefficient, versus_reward


def _terminal_bonus(
    a_dead: bool, b_dead: bool, win_bonus: float, loss_penalty: float
) -> float:
    """Terminal reward from the learning agent's perspective."""
    return versus_reward(0, 0, a_dead, b_dead, 0.0, win_bonus, loss_penalty)


# --- 상대 정책 ---
class VersusOpponent:
    """Decides one placement for a board. Return an encoded action in the schema domain or
    ``None`` only if the board has no legal move. The environment supplies a
    disposable clone, so mutations to the argument are never committed."""

    def reset(self) -> None:  # noqa: D401 - optional hook
        """Called on env reset. Override to reset per-episode state."""

    def reseed(self, seed: int) -> None:
        """Override when a custom opponent owns randomness used during an episode."""

    def act(self, sim: Any) -> Optional[int]:
        raise NotImplementedError


class RandomLegalOpponent(VersusOpponent):
    """Picks a uniformly random legal placement. Seeded for reproducibility."""

    def __init__(self, seed: int | None = None) -> None:
        self._rng = random.Random(seed)

    def reseed(self, seed: int) -> None:
        self._rng.seed(seed)

    def act(self, sim: Any) -> Optional[int]:
        placements = sim.legal_placements()
        if not placements:
            return None
        p = self._rng.choice(placements)
        return encode_action(int(p.col), int(p.rot))


class GreedyBCTSOpponent(VersusOpponent):
    """One-ply board evaluator: clone each legal placement and keep its best score.

    line_weight adds to the rows_cleared coefficient already in bcts_score.
    Equal scores keep the first native placement; terminal states have no
    separate priority. These are policy choices, not performance guarantees.
    """

    def __init__(self, line_weight: float = 1.0) -> None:
        self._line_weight = finite_coefficient(line_weight, "line_weight")

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
            if not np.isfinite(score):
                raise ValueError("greedy evaluation is not finite")
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
        obs = {k: v.numpy() for k, v in build_observation(sim).items()}
        mask = legal_mask(sim).numpy()
        if not mask.any():
            return None
        return action_index(self._policy_fn(obs, mask), NUM_PLACEMENTS)


# --- 대전 환경 ---
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
        self._initial_seed = optional_seed(seed)
        self._seed = self._initial_seed
        self._has_reset = False
        self._needs_reset = True
        self.render_mode = None
        # An explicit opponent board seed remains fixed across environment resets.
        self._configured_opp_seed = optional_seed(opponent_seed)
        self._opp_seed = self._configured_opp_seed
        self._opponent = opponent if opponent is not None else GreedyBCTSOpponent()

        self.attack_weight = finite_coefficient(attack_weight, "attack_weight")
        self.win_bonus = finite_coefficient(win_bonus, "win_bonus")
        self.loss_penalty = finite_coefficient(loss_penalty, "loss_penalty")
        # Legacy option name: the limit counts decisions, including blocked requests.
        self.max_pieces = positive_limit(max_pieces)
        if self.max_pieces is None:
            raise ValueError("max_pieces must be a positive decision budget")

        self.simA: Any = None  # agent board
        self.simB: Any = None  # opponent board
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
        if options is not None and (not isinstance(options, dict) or options):
            raise ValueError("reset options are not supported")
        seed = optional_seed(seed)
        effective = seed if seed is not None else (self._initial_seed if not self._has_reset else None)
        self._needs_reset = True  # A failing custom reset hook cannot leave a live episode.
        super().reset(seed=effective)
        self._seed = effective if effective is not None else draw_seed(self.np_random)
        self._opp_seed = (self._configured_opp_seed if self._configured_opp_seed is not None
                          else (self._seed + 1) % (1 << 64))
        self.simA = self._SimGame(self._seed)
        self.simB = self._SimGame(self._opp_seed)
        self._pieces = 0
        self._opponent.reseed(self._opp_seed)
        self._opponent.reset()
        self._has_reset = True
        self._needs_reset = False
        return self._observation(self.simA), self._info(0, 0)

    def step(
        self, action: int
    ) -> tuple[dict[str, np.ndarray], float, bool, bool, dict[str, Any]]:
        if self._needs_reset:
            raise gym.error.ResetNeeded("Call reset() before starting or continuing an episode")
        action = action_index(action, NUM_PLACEMENTS)
        # Preserve committed boards. A callback can advance its own RNG, so any
        # later failure requires reset rather than retrying an ambiguous episode.
        self._needs_reset = True
        candidate_a = self.simA.clone()
        candidate_b = self.simB.clone()
        col, rot = decode_action(action)
        attack_before_a = candidate_a.attack_lines_sent()
        cleared = candidate_a.apply_placement(col, rot)
        agent_attack = opp_attack = 0

        if cleared >= 0:
            agent_attack = candidate_a.attack_lines_sent() - attack_before_a
            if agent_attack > 0 and not candidate_b.game_over():
                candidate_b.add_pending_garbage(agent_attack)

            # The policy sees its own board after incoming attack has been queued.
            # Give it an isolated copy; policy-side mutations are never committed.
            if not candidate_b.game_over():
                opp_action = self._opponent.act(candidate_b.clone())
                mask = legal_mask(candidate_b).numpy()
                if opp_action is None:
                    if mask.any():
                        raise ValueError("opponent passed despite having legal actions")
                else:
                    opp_action = action_index(opp_action, NUM_PLACEMENTS)
                    if not mask[opp_action]:
                        raise ValueError("opponent returned a blocked action")
                    attack_before_b = candidate_b.attack_lines_sent()
                    bc, br = decode_action(opp_action)
                    if candidate_b.apply_placement(bc, br) < 0:
                        raise RuntimeError("opponent action disagreed with legal mask")
                    opp_attack = candidate_b.attack_lines_sent() - attack_before_b
                    if opp_attack > 0 and not candidate_a.game_over():
                        candidate_a.add_pending_garbage(opp_attack)

        a_dead, b_dead = candidate_a.game_over(), candidate_b.game_over()
        reward = (versus_reward(cleared, agent_attack, a_dead, b_dead,
                                self.attack_weight, self.win_bonus, self.loss_penalty)
                  if cleared >= 0 else 0.0)
        terminated = a_dead or b_dead
        decisions = self._pieces + 1
        truncated = decisions >= self.max_pieces
        obs = self._observation(candidate_a)
        info = self._info(agent_attack, opp_attack, sim_a=candidate_a,
                          sim_b=candidate_b, decisions=decisions)
        info["action_applied"] = cleared >= 0
        self.simA, self.simB = candidate_a, candidate_b
        self._pieces = decisions
        self._needs_reset = terminated or truncated
        return obs, reward, terminated, truncated, info

    def close(self) -> None:
        self.simA = self.simB = None
        self._needs_reset = True

    # --- 내부 헬퍼 ---
    def _observation(self, sim: Any) -> dict[str, np.ndarray]:
        return {k: v.numpy() for k, v in build_observation(sim).items()}

    def _info(self, agent_attack: int, opp_attack: int, *,
              sim_a=None, sim_b=None, decisions=None) -> dict[str, Any]:
        sim_a = self.simA if sim_a is None else sim_a
        sim_b = self.simB if sim_b is None else sim_b
        decisions = self._pieces if decisions is None else decisions
        assert sim_a is not None and sim_b is not None
        return {
            "legal_mask": legal_mask(sim_a).numpy(),
            "episode_seed": self._seed,
            "opponent_seed": self._opp_seed,
            "decisions": decisions,
            "score": sim_a.score(),
            "lines": sim_a.total_lines_cleared(),
            "state_hash": sim_a.state_hash(),
            "incoming_garbage": sim_a.pending_garbage(),
            "agent_attack": int(agent_attack),
            "opp_attack": int(opp_attack),
            "opp_alive": not sim_b.game_over(),
            "agent_lines": sim_a.total_lines_cleared(),
            "opp_lines": sim_b.total_lines_cleared(),
        }
