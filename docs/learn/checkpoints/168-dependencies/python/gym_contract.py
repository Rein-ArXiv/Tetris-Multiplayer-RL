"""Validation helpers for seed, action and limit contracts."""
from numbers import Integral

import numpy as np

_UINT64_MAX = (1 << 64) - 1


def _integer_scalar(value, what):
    """Return value as a Python int, rejecting bool, floats, strings and non-integer arrays."""
    if isinstance(value, (bool, np.bool_)):
        raise ValueError("%s must not be boolean" % what)
    if isinstance(value, np.ndarray):
        if value.ndim != 0 or not np.issubdtype(value.dtype, np.integer):
            raise ValueError("%s array must be a zero-dimensional integer array" % what)
        value = value.item()
    if not isinstance(value, Integral):
        raise ValueError("%s must be an integral value" % what)
    return int(value)


def optional_seed(value):
    """Return None or a Python int in [0, 2**64)."""
    if value is None:
        return None
    seed = _integer_scalar(value, "seed")
    if seed < 0 or seed > _UINT64_MAX:
        raise ValueError("seed must fit in uint64")
    return seed


def action_index(value, count):
    # The action domain is 0 <= index < count; legality may also depend on a
    # state-dependent legal-action mask that this helper does not model.
    if isinstance(count, (bool, np.bool_)) or not isinstance(count, Integral):
        raise ValueError("count must be a positive integer")
    icount = int(count)
    if icount <= 0:
        raise ValueError("count must be positive")
    index = _integer_scalar(value, "action index")
    if index < 0 or index >= icount:
        raise ValueError("action index out of range")
    return index


def positive_limit(value):
    """Return None or a positive Python int."""
    if value is None:
        return None
    limit = _integer_scalar(value, "limit")
    if limit <= 0:
        raise ValueError("limit must be positive")
    return limit


def draw_seed(rng):
    # Seeded RNG output is for repeatable experiments only, never credentials.
    return int(rng.integers(0, 1 << 64, dtype=np.uint64))
