"""An ordered two-board decision: A places, then B responds, then commit."""
import numpy as np
import gymnasium as gym
from gym_env import RoundEnv
from gym_contract import action_index, optional_seed
from actions import legal_mask
from observation import observe
from versus_reward import finite_coefficient, versus_reward


class RandomOpponent:
    """Own the opponent's sampling RNG; the environment reseeds each episode."""
    def reseed(self, seed):
        self._rng = np.random.default_rng(seed)

    def reset(self):
        pass

    def act(self, obs, mask):
        actions = np.flatnonzero(mask)
        return int(self._rng.choice(actions)) if actions.size else None


class VersusEnv(RoundEnv):
    def __init__(self, *, opponent=None, opponent_seed=None, attack_weight=0.5,
                 win_bonus=10., loss_penalty=10., **kwargs):
        if 'reward_spec' in kwargs:
            raise ValueError('versus uses its own decision-level reward contract')
        super().__init__(**kwargs)
        self._opponent = opponent if opponent is not None else RandomOpponent()
        self._fixed_other_seed = optional_seed(opponent_seed)
        self._other = None
        self._attack_weight = finite_coefficient(attack_weight, 'attack_weight')
        self._win_bonus = finite_coefficient(win_bonus, 'win_bonus')
        self._loss_penalty = finite_coefficient(loss_penalty, 'loss_penalty')

    def _versus_output(self, a, b, decisions, applied=None, attack_a=0, attack_b=0,
                       ticks_a=0, ticks_b=0, lines=0):
        obs, info = self._output(a, self._episode_seed, decisions, applied, ticks_a)
        # A joint decision has two separate logical clocks, not one shared tick.
        info.pop('discount')
        info.pop('reward_terms')
        info.update(opponent_seed=self._other_seed, opponent_state=b.state_bytes(),
                    agent_attack=attack_a, opp_attack=attack_b, lines_cleared=lines,
                    opponent_ticks=ticks_b, incoming_garbage=a.pending_garbage(),
                    opp_alive=not b.finished())
        return obs, info

    def reset(self, *, seed=None, options=None):
        super().reset(seed=seed, options=options)
        self._needs_reset = True
        self._other_seed = (self._fixed_other_seed if self._fixed_other_seed is not None
                            else (self._episode_seed + 1) % (1 << 64))
        other = self._sim.Session(self._other_seed, self._interval)
        self._opponent.reseed(self._other_seed)
        self._opponent.reset()
        result = self._versus_output(self._session, other, 0)
        self._other = other
        self._needs_reset = False
        return result

    def _opponent_action(self, board, mask):
        return self._opponent.act(observe(board, self._observation_schema), mask.copy())

    def step(self, action):
        if self._needs_reset:
            raise gym.error.ResetNeeded('reset required before a versus decision')
        action = action_index(action, self.action_space.n)
        self._needs_reset = True
        a, b = self._session.clone(), self._other.clone()
        applied = bool(legal_mask(a, self._action_schema)[action])
        attack_a = attack_b = ticks_a = ticks_b = lines = 0
        if applied:
            before_a = a.attack_sent()
            event = a.apply_action(action)
            ticks_a, lines = event['ticks'], event['lines']
            attack_a = a.attack_sent() - before_a
            if attack_a and not b.finished():
                b.add_garbage(attack_a)
            if not b.finished():
                # Keep authoritative legality separate from the policy's mutable arrays.
                mask = legal_mask(b, self._action_schema)
                choice = self._opponent_action(b, mask)
                if choice is None:
                    if mask.any():
                        raise ValueError('opponent passed despite legal actions')
                else:
                    choice = action_index(choice, self.action_space.n)
                    if not mask[choice]:
                        raise ValueError('opponent returned a blocked action')
                    before_b = b.attack_sent()
                    event = b.apply_action(choice)
                    ticks_b = event['ticks']
                    attack_b = b.attack_sent() - before_b
                    if attack_b and not a.finished():
                        a.add_garbage(attack_b)
        terminated = a.finished() or b.finished()
        reward = (versus_reward(lines, attack_a, a.finished(), b.finished(),
                                self._attack_weight, self._win_bonus, self._loss_penalty)
                  if applied else 0.)
        steps = self._steps + 1
        truncated = self._max_steps is not None and steps >= self._max_steps
        obs, info = self._versus_output(a, b, steps, applied, attack_a, attack_b,
                                       ticks_a, ticks_b, lines)
        self._session, self._other, self._steps = a, b, steps
        self._needs_reset = terminated or truncated
        return obs, reward, terminated, truncated, info

    def close(self):
        super().close()
        self._other = None
