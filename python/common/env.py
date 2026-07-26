"""Gymnasium-compatible environment wrapping ``SimGame``.

Exposes the placement-level action space (40 discrete actions ==
10 cols * 4 rotations) so external RL frameworks (CleanRL, SB3, LightZero,
RLlib) can train on Tetris without bespoke glue code.

Action / observation contract::

    action_space      = Discrete(40)         # encode_action(col, rot)
    observation_space = Dict(
        board   = Box(0, 1, (1, 20, 10), float32),
        current = Box(0, 1, (7,),        float32),
        next    = Box(0, 1, (7,),        float32),
    )
    info["legal_mask"] = bool array of shape (40,)  # set on every step

The reward is the number of lines cleared by the placement (0..4). Illegal
placements are masked out via ``info["legal_mask"]``; the env still tolerates
them defensively (returns 0 reward and does not advance the sim) so a buggy
exploration policy doesn't crash the rollout.
"""

from __future__ import annotations

from typing import Any

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
from .action_mask import decode_action, legal_mask
from .obs import build_observation


class TetrisPlacementEnv(gym.Env if _HAS_GYM else object):  # type: ignore[misc]
    """Single-player Tetris environment exposing placement-level actions."""

    metadata = {"render_modes": []}

    def __init__(self, seed: int | None = None) -> None:
        if not _HAS_GYM:
            raise ImportError(
                "gymnasium is required for TetrisPlacementEnv. "
                "Install it with `pip install gymnasium`."
            )
        # 여기서 import하는 이유가 있다. 최상단에서 하면 tetris_py를 빌드하지 않은
        # 환경에서 common 패키지 자체를 import할 수 없게 된다.
        # 가짜 SimGame을 끼워 넣는 테스트도 이 지연 import 덕분에 가능하다.
        from sim import SimGame  # noqa: PLC0415

        self._SimGame = SimGame
        self._seed = seed if seed is not None else 0
        self.sim: SimGame | None = None

        self.action_space = spaces.Discrete(NUM_PLACEMENTS)
        self.observation_space = spaces.Dict(
            {
                "board": spaces.Box(
                    low=0.0, high=1.0,
                    shape=(1, BOARD_ROWS, BOARD_COLS),
                    dtype=np.float32,
                ),
                "current": spaces.Box(
                    low=0.0, high=1.0,
                    shape=(NUM_PIECE_TYPES,),
                    dtype=np.float32,
                ),
                "next": spaces.Box(
                    low=0.0, high=1.0,
                    shape=(NUM_PIECE_TYPES,),
                    dtype=np.float32,
                ),
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
        self.sim = self._SimGame(self._seed)
        return self._observation(), self._info()

    def step(
        self, action: int
    ) -> tuple[dict[str, np.ndarray], float, bool, bool, dict[str, Any]]:
        assert self.sim is not None, "Call reset() before step()"

        col, rot = decode_action(int(action))
        cleared = self.sim.apply_placement(col, rot)

        if cleared < 0:
            # 불법 수가 오면 판을 건드리지 않고 보상 0만 돌려준다.
            # legal_mask를 제대로 쓰면 여기 올 일이 없지만, 마스킹을 빠뜨린
            # 학습 코드가 조용히 이상한 상태로 가는 것보다는 낫다.
            reward = 0.0
            terminated = self.sim.game_over()
        else:
            reward = float(cleared)
            terminated = self.sim.game_over()

        truncated = False
        return self._observation(), reward, terminated, truncated, self._info()

    # --- 내부 헬퍼 ---
    def _observation(self) -> dict[str, np.ndarray]:
        assert self.sim is not None
        obs = build_observation(self.sim)
        # Gymnasium은 관측을 NumPy 배열로 받는다. 여기서 텐서로 바꾸면
        # 환경을 감싸는 wrapper들이 깨진다. 텐서 변환은 학습 루프의 몫이다.
        return {k: v.numpy() for k, v in obs.items()}

    def _info(self) -> dict[str, Any]:
        assert self.sim is not None
        return {
            "legal_mask": legal_mask(self.sim).numpy(),
            "score": self.sim.score(),
            "state_hash": self.sim.state_hash(),
        }
