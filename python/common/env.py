"""Gymnasium environment over SimGame's instant placement API.

Spaces derive from the shared observation/action schema. An in-domain blocked
placement is a zero-reward self-loop; malformed or out-of-domain actions raise.
Explicit reset seeds reproduce a native episode; later unseeded resets draw
new native seeds from the environment RNG. The action space has its own RNG.
Use a TimeLimit wrapper when an external decision budget is required.
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
from .gym_contract import optional_seed, action_index, draw_seed


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
        self._initial_seed = optional_seed(seed)
        self._seed = self._initial_seed
        self._has_reset = False
        self._needs_reset = True
        self.render_mode = None
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
        if options is not None and (not isinstance(options, dict) or options):
            raise ValueError("reset options are not supported")
        seed = optional_seed(seed)
        effective = seed if seed is not None else (self._initial_seed if not self._has_reset else None)
        super().reset(seed=effective)
        self._seed = effective if effective is not None else draw_seed(self.np_random)
        self.sim = self._SimGame(self._seed)
        self._has_reset = True
        self._needs_reset = False
        return self._observation(), self._info()

    def step(
        self, action: int
    ) -> tuple[dict[str, np.ndarray], float, bool, bool, dict[str, Any]]:
        if self._needs_reset:
            raise gym.error.ResetNeeded("Call reset() before starting or continuing an episode")
        action = action_index(action, NUM_PLACEMENTS)
        col, rot = decode_action(action)
        cleared = self.sim.apply_placement(col, rot)

        if cleared < 0:
            # 도메인 안이지만 현재 보드에서 막힌 배치는 상태를 보존한다.
            # 정상 배치도 줄을 지우지 않으면 보상이 0이므로 info로 구별한다.
            reward = 0.0
            terminated = self.sim.game_over()
        else:
            reward = float(cleared)
            terminated = self.sim.game_over()

        truncated = False
        self._needs_reset = terminated
        info = self._info()
        info["action_applied"] = cleared >= 0
        return self._observation(), reward, terminated, truncated, info

    def close(self) -> None:
        self.sim = None
        self._needs_reset = True

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
            "episode_seed": self._seed,
            "score": self.sim.score(),
            "lines": self.sim.total_lines_cleared(),
            "state_hash": self.sim.state_hash(),
        }
