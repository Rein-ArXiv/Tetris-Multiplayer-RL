import math
from numbers import Real

import torch

__all__ = ['gae_targets', 'normalize_advantages']


def _check_vector(name, t):
    if not isinstance(t, torch.Tensor):
        raise TypeError(f'{name} must be a torch.Tensor, got {type(t).__name__}')
    if t.dim() != 1:
        raise ValueError(f'{name} must be 1D, got shape {tuple(t.shape)}')
    if t.numel() == 0:
        raise ValueError(f'{name} must be nonempty')


@torch.no_grad()
def gae_targets(rewards, values, next_values, discounts, terminated, boundaries, lam):
    """Detached GAE with distinct bootstrap and episode-trace boundaries.

    next_values belongs to the actual post-step observation, before reset.
    Only terminated removes that bootstrap; boundaries also cuts traces at
    truncation/reset. Final carry zero handles a rollout ending mid-episode.
    discounts uses the reward's transition clock; lambda decays per decision.
    All tensors are nonempty 1D and finite, with matching dtype/device.
    """
    if isinstance(lam, bool):
        raise TypeError('lam must be a real number, not a bool')
    if not isinstance(lam, Real):
        raise TypeError(f'lam must be a real number, got {type(lam).__name__}')
    lam = float(lam)
    if not math.isfinite(lam):
        raise ValueError('lam must be finite')
    if not (0.0 <= lam <= 1.0):
        raise ValueError(f'lam must be in [0, 1], got {lam}')

    float_names = ('rewards', 'values', 'next_values', 'discounts')
    float_tensors = (rewards, values, next_values, discounts)
    bool_names = ('terminated', 'boundaries')
    bool_tensors = (terminated, boundaries)

    for name, t in zip(float_names, float_tensors):
        _check_vector(name, t)
        if not t.is_floating_point():
            raise TypeError(f'{name} must be a floating dtype, got {t.dtype}')
    for name, t in zip(bool_names, bool_tensors):
        _check_vector(name, t)
        if t.dtype != torch.bool:
            raise TypeError(f'{name} must be torch.bool, got {t.dtype}')

    length = rewards.shape[0]
    for name, t in zip(float_names[1:] + bool_names, float_tensors[1:] + bool_tensors):
        if t.shape[0] != length:
            raise ValueError(
                f'all inputs must share length {length}, but {name} has length {t.shape[0]}'
            )

    device = values.device
    dtype = values.dtype
    for name, t in zip(float_names, float_tensors):
        if t.device != device:
            raise ValueError(f'{name} must be on device {device}, got {t.device}')
        if t.dtype != dtype:
            raise ValueError(f'{name} must have dtype {dtype}, got {t.dtype}')
    for name, t in zip(bool_names, bool_tensors):
        if t.device != device:
            raise ValueError(f'{name} must be on device {device}, got {t.device}')

    for name, t in zip(float_names, float_tensors):
        if not bool(torch.isfinite(t).all()):
            raise ValueError(f'{name} must be finite')
    if not bool(((discounts >= 0.0) & (discounts <= 1.0)).all()):
        raise ValueError('discounts must lie in [0, 1]')
    if bool((terminated & ~boundaries).any()):
        raise ValueError('every terminated transition must also be a boundary')

    zeros = torch.zeros((), dtype=dtype, device=device)
    bootstraps = torch.where(terminated, zeros, next_values)
    deltas = rewards + discounts * bootstraps - values

    advantages = torch.empty_like(values)
    carry = zeros
    for t in range(length - 1, -1, -1):
        carry = deltas[t] + discounts[t] * lam * torch.where(boundaries[t], zeros, carry)
        advantages[t] = carry

    returns = advantages + values
    if not torch.isfinite(advantages).all() or not torch.isfinite(returns).all():
        raise ValueError('GAE targets overflowed')
    return advantages, returns


def _check_advantages(advantages):
    if not isinstance(advantages, torch.Tensor):
        raise TypeError(f'advantages must be a torch.Tensor, got {type(advantages).__name__}')
    if advantages.dim() != 1:
        raise ValueError(f'advantages must be 1D, got shape {tuple(advantages.shape)}')
    if advantages.numel() == 0:
        raise ValueError('advantages must be nonempty')
    if not advantages.is_floating_point():
        raise TypeError(f'advantages must be floating, got {advantages.dtype}')
    if not bool(torch.isfinite(advantages).all()):
        raise ValueError('advantages must be finite')


@torch.no_grad()
def normalize_advantages(advantages):
    '''Center advantages and scale by their population standard deviation.'''
    _check_advantages(advantages)
    if advantages.numel() == 1:
        return advantages.clone()
    mean = advantages.mean()
    std = advantages.std(unbiased=False)
    result = (advantages - mean) / (std + 1e-8)
    if not torch.isfinite(mean) or not torch.isfinite(std) or not torch.isfinite(result).all():
        raise ValueError('advantage normalization overflowed')
    return result
