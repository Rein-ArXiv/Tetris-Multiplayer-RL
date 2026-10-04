"""Reward components at an action boundary, with an explicit discount clock.

Line reward and terminal penalty occur on the final tick. Potential shaping
uses the same continuation discount as the return, and true terminals have
zero after-potential. A collection cutoff is not a true terminal here.
"""
from __future__ import annotations

import math
from dataclasses import dataclass


@dataclass(frozen=True)
class RewardSpec:
    gamma: float = 0.99
    shaping_scale: float = 0.1
    line_weight: float = 1.0
    terminal_penalty: float = 0.0
    time_basis: str = 'decision'

    def __post_init__(self) -> None:
        _require_finite_number('gamma', self.gamma)
        _require_finite_number('shaping_scale', self.shaping_scale)
        _require_finite_number('line_weight', self.line_weight)
        _require_finite_number('terminal_penalty', self.terminal_penalty)

        if not (0.0 < self.gamma <= 1.0):
            raise ValueError('gamma must satisfy 0 < gamma <= 1')
        if self.shaping_scale < 0.0:
            raise ValueError('shaping_scale must be >= 0')
        if self.line_weight < 0.0:
            raise ValueError('line_weight must be >= 0')
        if self.terminal_penalty < 0.0:
            raise ValueError('terminal_penalty must be >= 0')
        if self.time_basis not in ('decision', 'tick'):
            raise ValueError('time_basis must be decision or tick')


@dataclass(frozen=True)
class RewardParts:
    base: float
    shaping: float
    terminal: float
    total: float
    discount: float


def _require_finite_number(name: str, value) -> None:
    if isinstance(value, bool) or not isinstance(value, (int, float)):
        raise TypeError(name + ' must be a real number, not bool')
    try:
        finite = math.isfinite(value)
    except OverflowError as exc:
        raise ValueError(name + ' is not representable') from exc
    if not finite:
        raise ValueError(name + ' must be finite')


def _require_int(name: str, value) -> None:
    if isinstance(value, bool) or not isinstance(value, int):
        raise TypeError(name + ' must be an integer, not bool')


def evaluate(lines, ticks, terminated, before_potential, after_potential, spec):
    if not isinstance(spec, RewardSpec):
        raise TypeError('spec must be a RewardSpec')
    if not isinstance(terminated, bool):
        raise TypeError('terminated must be a bool')

    _require_int('lines', lines)
    _require_int('ticks', ticks)
    if lines < 0:
        raise ValueError('lines must be >= 0')
    if ticks <= 0:
        raise ValueError('ticks must be > 0')

    _require_finite_number('before_potential', before_potential)
    _require_finite_number('after_potential', after_potential)

    gamma = spec.gamma

    if spec.time_basis == 'decision':
        discount = gamma
        timing_factor = 1.0
    else:
        discount = gamma ** ticks
        timing_factor = gamma ** (ticks - 1)

    if not math.isfinite(discount):
        raise ValueError('discount is not finite')

    try:
        base = spec.line_weight * lines * timing_factor
    except OverflowError as exc:
        raise ValueError('line reward is not representable') from exc
    terminal = (-spec.terminal_penalty if terminated else 0.0) * timing_factor

    bootstrap_potential = 0.0 if terminated else after_potential
    shaping = (spec.shaping_scale * (discount * bootstrap_potential - before_potential)
               if spec.shaping_scale else 0.0)

    total = base + shaping + terminal
    for label, value in (('base', base), ('shaping', shaping), ('terminal', terminal), ('total', total)):
        if not math.isfinite(value):
            raise ValueError(label + ' reward is not finite')

    return RewardParts(
        base=base,
        shaping=shaping,
        terminal=terminal,
        total=total,
        discount=discount,
    )
