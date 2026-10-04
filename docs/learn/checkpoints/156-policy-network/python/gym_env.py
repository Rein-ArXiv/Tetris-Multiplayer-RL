"""Gym adapter around the cumulative Session, observation and reward contracts."""
from dataclasses import asdict
import math

import gymnasium as gym
from gymnasium import spaces
import numpy as np

from gym_contract import optional_seed, action_index, positive_limit, draw_seed
from observation import observe
from actions import legal_mask
from rewards import RewardSpec, RewardParts
from reward_runner import execute, board_potential


class RoundEnv(gym.Env):
    metadata = {'render_modes': []}

    def __init__(self, *, seed=None, gravity_interval=30, max_steps=None,
                 reward_spec=RewardSpec()):
        import study_py as sim
        self._sim = sim
        self._initial_seed = optional_seed(seed)
        self._interval = positive_limit(gravity_interval)
        if self._interval is None:
            raise ValueError('gravity interval required')
        self._max_steps = positive_limit(max_steps)
        if not isinstance(reward_spec, RewardSpec):
            raise TypeError('RewardSpec required')
        self._reward_spec = reward_spec  # Env.spec is reserved for Gym registration.
        self._observation_schema = sim.observation_schema()
        self._action_schema = sim.action_schema()
        schema = self._observation_schema
        self.action_space = spaces.Discrete(self._action_schema['count'])
        self.observation_space = spaces.Dict({
            'board': spaces.Box(0, 1, (1, schema['rows'], schema['cols']), np.float32),
            'current': spaces.Box(0, 1, (len(schema['piece_ids']),), np.float32),
            'next': spaces.Box(0, 1, (len(schema['piece_ids']),), np.float32),
        })
        self.render_mode = None
        self._session = None
        self._has_reset = False
        self._needs_reset = True
        self._steps = 0
        self._episode_seed = None

    def _output(self, session, seed, steps, applied=None, ticks=0, parts=None):
        obs = observe(session, self._observation_schema)
        if not self.observation_space.contains(obs):
            raise RuntimeError('observation outside the declared schema')
        info = {
            'legal_mask': legal_mask(session, self._action_schema),
            'episode_seed': seed,
            'decisions': steps,
            'action_applied': applied,
            'ticks': ticks,
            'score': session.score(),
            'state_bytes': session.state_bytes(),
            'discount': parts.discount if parts else 1.0,
            'reward_terms': asdict(parts) if parts else None,
        }
        return obs, info

    def reset(self, *, seed=None, options=None):
        if options is not None and (not isinstance(options, dict) or options):
            raise ValueError('reset options are not supported')
        seed = optional_seed(seed)
        effective = seed
        if effective is None and not self._has_reset:
            effective = self._initial_seed
        super().reset(seed=effective)
        episode_seed = effective if effective is not None else draw_seed(self.np_random)
        candidate = self._sim.Session(episode_seed, self._interval)
        obs, info = self._output(candidate, episode_seed, 0)
        self._session = candidate
        self._episode_seed = episode_seed
        self._steps = 0
        self._has_reset = True
        self._needs_reset = False
        return obs, info

    def _blocked_reward(self):
        # A domain-valid blocked request is a decision with zero simulation ticks.
        spec = self._reward_spec
        discount = spec.gamma if spec.time_basis == 'decision' else 1.0
        potential = board_potential(self._session.grid())
        shaping = spec.shaping_scale * ((discount - 1.0) * potential)
        if not math.isfinite(shaping):
            raise ValueError('blocked reward is not finite')
        return RewardParts(0.0, shaping, 0.0, shaping, discount)

    def step(self, action):
        if self._needs_reset:
            raise gym.error.ResetNeeded('Call reset before step or after an episode ends')
        action = action_index(action, self.action_space.n)
        mask = legal_mask(self._session, self._action_schema)
        applied = bool(mask[action])
        if applied:
            candidate, transition = execute(self._session, action, self._reward_spec)
            parts, ticks = transition.reward, transition.ticks
        else:
            candidate = self._session.clone()
            parts, ticks = self._blocked_reward(), 0
        steps = self._steps + 1
        terminated = candidate.finished()
        truncated = self._max_steps is not None and steps >= self._max_steps
        obs, info = self._output(candidate, self._episode_seed, steps, applied, ticks, parts)
        # Commit only after transition, reward and output construction succeed.
        self._session = candidate
        self._steps = steps
        self._needs_reset = terminated or truncated
        return obs, float(parts.total), bool(terminated), bool(truncated), info

    def close(self):
        self._session = None
        self._needs_reset = True
