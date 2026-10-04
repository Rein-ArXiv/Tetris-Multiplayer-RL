"""Single-transition reward for a versus battle.

The returned value is the reward for exactly one transition. Callers must
never step a terminated episode, so that no reward is attributed to a
transition that does not exist. No I/O or framework dependencies.
"""

import math
import numbers


def finite_coefficient(value, name):
    """Validate a real coefficient and return it as ``float``.

    Booleans are rejected. Non-finite values, and values that overflow or
    otherwise fail conversion to ``float``, raise ``ValueError``.
    """
    if isinstance(value, bool) or not isinstance(value, numbers.Real):
        raise ValueError(
            "{} must be a finite real number, got {}".format(name, type(value).__name__)
        )
    try:
        result = float(value)
    except (TypeError, ValueError, OverflowError) as exc:
        raise ValueError("{} cannot be converted to float: {!r}".format(name, value)) from exc
    if not math.isfinite(result):
        raise ValueError("{} must be finite, got {!r}".format(name, value))
    return result


def versus_reward(lines, attack, a_dead, b_dead, attack_weight, win_bonus, loss_penalty):
    """Return the reward for one transition of a versus battle.

    ``lines`` and ``attack`` are non-negative integer magnitudes; the base
    reward is ``float(lines) + attack_weight * float(attack)``. ``win_bonus``
    is added only when B is dead and A is alive; ``loss_penalty`` is
    subtracted whenever A is dead (including a double knockout). The three
    coefficients may be negative: the sign is an experiment choice, so only
    their type and finiteness are validated.
    """
    for value, name in ((lines, "lines"), (attack, "attack")):
        if isinstance(value, bool) or not isinstance(value, numbers.Integral):
            raise ValueError(
                "{} must be a non-negative integer, got {}".format(
                    name, type(value).__name__
                )
            )
        if value < 0:
            raise ValueError("{} must be non-negative, got {!r}".format(name, value))

    for value, name in ((a_dead, "a_dead"), (b_dead, "b_dead")):
        if not isinstance(value, bool):
            raise ValueError(
                "{} must be a bool, got {}".format(name, type(value).__name__)
            )

    weight = finite_coefficient(attack_weight, "attack_weight")
    win = finite_coefficient(win_bonus, "win_bonus")
    loss = finite_coefficient(loss_penalty, "loss_penalty")

    try:
        reward = float(lines) + weight * float(attack)
    except OverflowError as exc:
        raise ValueError("reward overflowed while converting inputs") from exc

    if b_dead and not a_dead:
        reward += win
    if a_dead:
        reward -= loss

    if not math.isfinite(reward):
        raise ValueError("reward is not finite: {!r}".format(reward))
    return reward
