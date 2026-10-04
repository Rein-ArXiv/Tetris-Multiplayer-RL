"""CPU PPO snapshots at a completed-update barrier, including an active episode.

Only this synchronous single-environment trainer owns the saved state. A failed
advance poisons the run: a partially applied optimizer update is not resumable.
"""
from copy import deepcopy
from dataclasses import asdict, dataclass
import math
import platform
import sys
import gymnasium
import numpy as np
import torch
from atomic_save import atomic_torch_save
from model_contract import positive_size
from policy_network import PolicySchema, PolicyNet
from ppo import Collector, update, discount_value
from replay_env import ReplayRoundEnv
from rewards import RewardSpec

FORMAT_VERSION = 1
RULES_REVISION = 'study-round-cp158-v1'  # Manual rule/encoding compatibility revision.


@dataclass(frozen=True)
class Config:
    seed: int = 158
    env_seed: int = 11
    rollout: int = 5
    max_steps: int = 7
    gravity_interval: int = 30
    gamma: float = .99
    lam: float = .95
    lr: float = .0003
    epochs: int = 2
    minibatch: int = 3
    conv_channels: tuple = (4, 8)
    hidden: int = 32

    def __post_init__(self):
        for name in ('seed', 'env_seed'):
            value = getattr(self, name)
            if type(value) is not int or not 0 <= value < 2**63:
                raise ValueError(name + ' must be a nonnegative signed-64 seed')
        for name in ('rollout', 'max_steps', 'gravity_interval', 'epochs', 'minibatch', 'hidden'):
            positive_size(getattr(self, name), name)
        discount_value(self.gamma)
        discount_value(self.lam)
        if type(self.lr) not in (int, float) or not math.isfinite(self.lr) or self.lr <= 0:
            raise ValueError('lr must be finite and positive')
        if not isinstance(self.conv_channels, (tuple, list)) or not self.conv_channels:
            raise ValueError('conv_channels must be nonempty')
        object.__setattr__(self, 'conv_channels', tuple(positive_size(c, 'channel') for c in self.conv_channels))


def runtime_contract(sim):
    # Descriptive equality gate; it cannot prove equal hardware/math behavior.
    return dict(rules=RULES_REVISION, observation=sim.observation_schema(), action=sim.action_schema(),
                python=sys.version.split()[0], torch=str(torch.__version__),
                numpy=np.__version__, gym=gymnasium.__version__, machine=platform.machine())


def finite_tree(value):
    if isinstance(value, torch.Tensor):
        if value.device.type != 'cpu' or value.layout != torch.strided or not torch.isfinite(value).all():
            raise ValueError('snapshot tensor must be finite dense CPU data')
    elif isinstance(value, dict):
        for item in value.values(): finite_tree(item)
    elif isinstance(value, (list, tuple)):
        for item in value: finite_tree(item)
    elif isinstance(value, float) and not math.isfinite(value):
        raise ValueError('snapshot number must be finite')


class TrainingRun:
    def __init__(self, config=Config()):
        import study_py as sim
        self.config = config
        self.contract = runtime_contract(sim)
        torch.manual_seed(config.seed)
        self.model = PolicyNet(PolicySchema.from_native(sim.observation_schema(), sim.action_schema()),
                               conv_channels=config.conv_channels, hidden=config.hidden)
        self.optimizer = torch.optim.Adam(self.model.parameters(), lr=config.lr)
        env = ReplayRoundEnv(gravity_interval=config.gravity_interval, max_steps=config.max_steps,
                             reward_spec=RewardSpec(gamma=config.gamma))
        self.collector = Collector(env, seed=config.env_seed, fallback_discount=config.gamma)
        self.decisions = self.updates = 0
        self.ready = True

    def advance(self, count=None):
        if not self.ready:
            raise RuntimeError('run is busy or a previous update failed')
        count = self.config.rollout if count is None else positive_size(count, 'count')
        self.ready = False
        self.model.eval()
        data = self.collector.collect(self.model, count, device='cpu', lam=self.config.lam)
        metrics = update(self.model, self.optimizer, data, epochs=self.config.epochs,
                         minibatch=self.config.minibatch)
        self.optimizer.zero_grad(set_to_none=True)  # No pending gradients at this barrier.
        self.decisions += len(data['action'])
        self.updates += 1
        self.ready = True
        return data, metrics

    def save(self, path):
        if not self.ready:
            raise RuntimeError('save only after a completed update')
        c = self.collector
        payload = dict(format_version=FORMAT_VERSION, contract=self.contract,
                       config=asdict(self.config), model=self.model.state_dict(),
                       optimizer=self.optimizer.state_dict(), model_training=self.model.training,
                       decisions=self.decisions, updates=self.updates,
                       torch_rng=torch.get_rng_state(), environment=c.env.checkpoint(),
                       collector=dict(episode_reward=c.episode_reward, episode_lines=c.episode_lines,
                                      episode_decisions=c.episode_decisions, completed=deepcopy(c.completed)))
        finite_tree(payload)
        atomic_torch_save(payload, path)

    @classmethod
    def load(cls, path):
        saved = torch.load(path, map_location='cpu', weights_only=True)
        import study_py as sim
        if (not isinstance(saved, dict) or type(saved.get('format_version')) is not int
                or saved['format_version'] != FORMAT_VERSION):
            raise ValueError('unsupported training snapshot format')
        if saved.get('contract') != runtime_contract(sim):
            raise ValueError('training runtime/rule/schema contract mismatch')
        finite_tree(saved)
        for name in ('decisions', 'updates'):
            if type(saved[name]) is not int or saved[name] < 0:
                raise ValueError('invalid progress counter')
        if type(saved['model_training']) is not bool:
            raise ValueError('invalid model mode')
        candidate = None
        # Construction initializes parameters and consumes RNG. Keep the caller's
        # generator unchanged on every rejected candidate, including bad RNG bytes.
        with torch.random.fork_rng(devices=[]):
            try:
                candidate = cls(Config(**saved['config']))
                expected = candidate.model.state_dict()
                if (set(saved['model']) != set(expected)
                        or any(saved['model'][k].shape != t.shape or saved['model'][k].dtype != t.dtype
                               for k,t in expected.items())):
                    raise ValueError('model tensor schema mismatch')
                candidate.model.load_state_dict(saved['model'], strict=True)
                expected_groups = [{k:v for k,v in g.items() if k != 'params'}
                                   for g in candidate.optimizer.param_groups]
                candidate.optimizer.load_state_dict(saved['optimizer'])
                if expected_groups != [{k:v for k,v in g.items() if k != 'params'}
                                       for g in candidate.optimizer.param_groups]:
                    raise ValueError('optimizer configuration mismatch')
                candidate._check_optimizer()
                if saved['updates'] and len(candidate.optimizer.state) != len(list(candidate.model.parameters())):
                    raise ValueError('missing Adam moments')
                candidate.collector.env.close()
                env, obs, info = ReplayRoundEnv.restore(saved['environment'])
                candidate.collector.env = env
                if (env._interval != candidate.config.gravity_interval
                        or env._max_steps != candidate.config.max_steps
                        or env._reward_spec != RewardSpec(gamma=candidate.config.gamma)):
                    raise ValueError('environment/config mismatch')
                c = candidate.collector
                c.obs, c.info = obs, info
                stats = saved['collector']
                if (type(stats['episode_decisions']) is not int
                        or stats['episode_decisions'] != len(env.actions) or env._needs_reset):
                    raise ValueError('collector episode boundary mismatch')
                if type(stats['episode_lines']) is not int or stats['episode_lines'] < 0:
                    raise ValueError('invalid episode lines')
                c.episode_reward = float(stats['episode_reward'])
                c.episode_lines = stats['episode_lines']
                c.episode_decisions = stats['episode_decisions']
                c.completed = deepcopy(stats['completed'])
                candidate.decisions, candidate.updates = saved['decisions'], saved['updates']
                candidate.model.train(saved['model_training'])
                torch.set_rng_state(saved['torch_rng'])  # Validate before committing externally.
            except BaseException:
                if candidate is not None: candidate.close()
                raise
        torch.set_rng_state(saved['torch_rng'])
        return candidate

    def _check_optimizer(self):
        groups = self.optimizer.param_groups
        if len(groups) != 1 or groups[0]['lr'] != self.config.lr:
            raise ValueError('optimizer configuration mismatch')
        for parameter, state in self.optimizer.state.items():
            if set(state) != {'step', 'exp_avg', 'exp_avg_sq'}:
                raise ValueError('unsupported Adam state')
            if (state['step'].numel() != 1 or float(state['step']) < 0
                    or any(state[k].shape != parameter.shape or state[k].dtype != parameter.dtype
                           for k in ('exp_avg', 'exp_avg_sq'))):
                raise ValueError('Adam state tensor schema mismatch')

    def close(self):
        self.collector.env.close()
