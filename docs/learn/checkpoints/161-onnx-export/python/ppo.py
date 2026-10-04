"""One synchronous environment: frozen rollout targets, PPO update, separate evaluation."""
import math
from numbers import Real
import torch
from actions import masked_log_softmax, entropy
from model_contract import positive_size
from returns import gae_targets, normalize_advantages
from ppo_loss import clipped_policy_loss


def batch_of(observation, device):
    # Own the samples even if an environment later reuses its output arrays.
    return {k: torch.as_tensor(observation[k], dtype=torch.float32, device=device)
                  .unsqueeze(0).clone() for k in ('board', 'current', 'next')}


def discount_value(value):
    if isinstance(value, bool) or not isinstance(value, Real) or not math.isfinite(value) or not 0 <= value <= 1:
        raise ValueError('discount must be finite and in [0,1]')
    return float(value)


class Collector:
    def __init__(self, env, *, seed, fallback_discount):
        self.env = env
        self.fallback_discount = discount_value(fallback_discount)
        self.obs, self.info = env.reset(seed=seed)
        self.episode_reward = 0.0
        self.episode_lines = 0
        self.episode_decisions = 0
        self.completed = []

    @torch.no_grad()
    def collect(self, model, count, *, device, lam):
        count = positive_size(count, 'count')
        rows = []
        for _ in range(count):
            obs = batch_of(self.obs, device)
            mask = torch.as_tensor(self.info['legal_mask'], dtype=torch.bool, device=device).unsqueeze(0).clone()
            logits, value = model(obs['board'], obs['current'], obs['next'])
            logp = masked_log_softmax(logits, mask)
            action = torch.multinomial(logp.exp(), 1)
            chosen_logp = logp.gather(1, action).squeeze(-1)
            next_obs, reward, terminated, truncated, next_info = self.env.step(int(action[0, 0]))
            endpoint_value = torch.zeros_like(value)
            if not terminated:
                endpoint = batch_of(next_obs, device)
                _, endpoint_value = model(endpoint['board'], endpoint['current'], endpoint['next'])
            discount = discount_value(next_info.get('discount', self.fallback_discount))
            row = {k: v[0].clone() for k, v in obs.items()}
            row.update(mask=mask[0], action=action[0, 0], old_logp=chosen_logp[0],
                       value=value[0], next_value=endpoint_value[0],
                       reward=torch.tensor(reward, dtype=value.dtype, device=device),
                       discount=torch.tensor(discount, dtype=value.dtype, device=device),
                       terminated=torch.tensor(terminated, device=device),
                       boundary=torch.tensor(terminated or truncated, device=device))
            rows.append(row)
            self.episode_reward += float(reward)
            self.episode_lines += int(next_info['lines_cleared'])
            self.episode_decisions += 1
            self.obs, self.info = next_obs, next_info
            if terminated or truncated:
                self.completed.append(dict(reward=self.episode_reward, lines=self.episode_lines,
                                           decisions=self.episode_decisions,
                                           terminated=terminated, truncated=truncated))
                self.episode_reward = 0.0
                self.episode_lines = self.episode_decisions = 0
                self.obs, self.info = self.env.reset()
        data = {k: torch.stack([row[k] for row in rows]) for k in rows[0]}
        data['advantage'], data['return'] = gae_targets(
            data['reward'], data['value'], data['next_value'], data['discount'],
            data['terminated'], data['boundary'], lam,
        )
        return data


def update(model, optimizer, data, *, epochs=2, minibatch=8, clip=0.2,
           value_coef=0.5, entropy_coef=0.01, max_grad_norm=0.5):
    epochs, minibatch = positive_size(epochs, 'epochs'), positive_size(minibatch, 'minibatch')
    for name, value in (('value_coef', value_coef), ('entropy_coef', entropy_coef),
                        ('max_grad_norm', max_grad_norm)):
        if isinstance(value, bool) or not isinstance(value, Real) or not math.isfinite(value) or value < 0:
            raise ValueError(name + ' must be finite and nonnegative')
    if max_grad_norm == 0:
        raise ValueError('max_grad_norm must be positive')
    size = len(data['action'])
    advantage = normalize_advantages(data['advantage'])
    sums = dict(policy=0.0, value=0.0, entropy=0.0, approximate_kl=0.0, clip_fraction=0.0)
    samples = 0
    model.train()
    for _ in range(epochs):
        order = torch.randperm(size, device=data['action'].device)
        for start in range(0, size, minibatch):
            index = order[start:start + minibatch]
            logits, value = model(data['board'][index], data['current'][index], data['next'][index])
            logp = masked_log_softmax(logits, data['mask'][index])
            selected = logp.gather(1, data['action'][index, None]).squeeze(-1)
            policy, kl, fraction = clipped_policy_loss(selected, data['old_logp'][index], advantage[index], clip)
            value_loss = 0.5 * (value - data['return'][index]).square().mean()
            exploration = entropy(logp, data['mask'][index]).mean()
            loss = policy + value_coef * value_loss - entropy_coef * exploration
            if not torch.isfinite(loss):
                raise ValueError('PPO loss must be finite')
            optimizer.zero_grad(set_to_none=True)
            loss.backward()
            torch.nn.utils.clip_grad_norm_(model.parameters(), max_grad_norm, error_if_nonfinite=True)
            optimizer.step()
            for key, metric in zip(sums, (policy, value_loss, exploration, kl, fraction)):
                sums[key] += float(metric.detach()) * len(index)
            samples += len(index)
    return {k: v / samples for k, v in sums.items()}


@torch.no_grad()
def evaluate(model, make_env, *, seeds, limit, device):
    limit = positive_size(limit, 'limit')
    was_training = model.training
    env = None
    records = []
    try:
        model.eval()
        env = make_env()
        for seed in seeds:
            obs, info = env.reset(seed=seed)
            reward_sum = 0.0
            lines = decisions = 0
            terminated = truncated = False
            while decisions < limit:
                batch = batch_of(obs, device)
                logits, _ = model(batch['board'], batch['current'], batch['next'])
                mask = torch.as_tensor(info['legal_mask'], dtype=torch.bool, device=device).unsqueeze(0)
                action = int(masked_log_softmax(logits, mask).argmax(-1)[0])
                obs, reward, terminated, truncated, info = env.step(action)
                reward_sum += float(reward)
                lines += int(info['lines_cleared'])
                decisions += 1
                if terminated or truncated:
                    break
            records.append(dict(seed=seed, reward=reward_sum, lines=lines, score=info['score'],
                                decisions=decisions, terminated=terminated, truncated=truncated,
                                evaluation_cutoff=decisions == limit and not (terminated or truncated)))
    finally:
        model.train(was_training)
        if env is not None:
            env.close()
    return records
