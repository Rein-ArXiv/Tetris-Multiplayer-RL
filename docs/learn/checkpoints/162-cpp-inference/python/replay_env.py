"""Persist a RoundEnv by replaying the current episode's successful decisions."""
from copy import deepcopy
from dataclasses import asdict
from gym_env import RoundEnv
from rewards import RewardSpec


class ReplayRoundEnv(RoundEnv):
    def reset(self, *, seed=None, options=None):
        result = super().reset(seed=seed, options=options)
        self.actions = []
        return result

    def step(self, action):
        result = super().step(action)
        self.actions.append(int(action))  # Record only a successfully committed step.
        return result

    def checkpoint(self):
        if self._session is None or not self._has_reset:
            raise RuntimeError('checkpoint requires an initialized environment')
        return dict(seed=self._episode_seed, actions=list(self.actions),
                    state_bytes=self._session.state_bytes(),
                    rng=deepcopy(self.np_random.bit_generator.state),
                    gravity_interval=self._interval, max_steps=self._max_steps,
                    reward_spec=asdict(self._reward_spec))

    @classmethod
    def restore(cls, saved):
        env = cls(gravity_interval=saved['gravity_interval'], max_steps=saved['max_steps'],
                  reward_spec=RewardSpec(**saved['reward_spec']))
        try:
            obs, info = env.reset(seed=saved['seed'])
            for action in saved['actions']:
                obs, _, _, _, info = env.step(action)
            if env._session.state_bytes() != saved['state_bytes']:
                raise ValueError('environment replay differs from saved state')
            # Replaying a seeded episode does not reconstruct the generator used
            # to choose future episode seeds. Restore that generator separately.
            env.np_random.bit_generator.state = deepcopy(saved['rng'])
            return env, obs, info
        except BaseException:
            env.close()
            raise
