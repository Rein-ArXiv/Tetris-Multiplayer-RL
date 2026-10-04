'''Evaluation summaries for the Tetris learning course helper.'''

import math
import statistics

METRICS = ('lines', 'score', 'pieces', 'reward')
NONNEG_FIELDS = ('lines', 'score', 'pieces')
ENDINGS = ('terminated', 'truncated', 'budget')


def _nonneg_int(value, field, index):
    if isinstance(value, bool) or not isinstance(value, int):
        raise ValueError('episode %d: %s must be a nonnegative int' % (index, field))
    if value < 0:
        raise ValueError('episode %d: %s must be nonnegative' % (index, field))
    return value


def _reward(value, index):
    if isinstance(value, bool) or not isinstance(value, (int, float)):
        raise ValueError('episode %d: reward must be a real number' % index)
    if not math.isfinite(value):
        raise ValueError('episode %d: reward must be finite' % index)
    return value


def _validate_rows(rows):
    if not isinstance(rows, list) or not rows:
        raise ValueError('rows must be a nonempty list')
    seen = set()
    for index, row in enumerate(rows):
        if not isinstance(row, dict):
            raise ValueError('episode %d: must be a dict' % index)
        seed = row.get('seed')
        if isinstance(seed, bool) or not isinstance(seed, int) or seed < 0:
            raise ValueError('episode %d: seed must be a nonnegative int' % index)
        if seed in seen:
            raise ValueError('episode %d: duplicate seed %d' % (index, seed))
        seen.add(seed)
        for field in NONNEG_FIELDS:
            _nonneg_int(row.get(field), field, index)
        _reward(row.get('reward'), index)
        if row.get('end') not in ENDINGS:
            raise ValueError('episode %d: end must be one of %s' % (index, ', '.join(ENDINGS)))
    return rows


def _stats(values):
    return {
        'mean': statistics.mean(values),
        'median': statistics.median(values),
        'minimum': min(values),
        'maximum': max(values),
    }


def summarize(rows):
    _validate_rows(rows)
    metrics = {name: _stats([row[name] for row in rows]) for name in METRICS}
    endings = {name: 0 for name in ENDINGS}
    for row in rows:
        endings[row['end']] += 1
    return {'episodes': len(rows), 'metrics': metrics, 'endings': endings}


def paired_difference(left, right, metric='lines'):
    if metric not in METRICS:
        raise ValueError('metric must be one of %s' % ', '.join(METRICS))
    _validate_rows(left)
    _validate_rows(right)
    if [row['seed'] for row in left] != [row['seed'] for row in right]:
        raise ValueError('left and right must have the same ordered seeds')
    diffs = [r[metric] - l[metric] for l, r in zip(left, right)]
    if not all(math.isfinite(value) for value in diffs):
        raise ValueError("paired differences must be finite")
    return {
        'mean': statistics.mean(diffs),
        'median': statistics.median(diffs),
        'minimum': min(diffs),
        'maximum': max(diffs),
        'positive_seeds': sum(1 for d in diffs if d > 0),
        'negative_seeds': sum(1 for d in diffs if d < 0),
        'tied_seeds': sum(1 for d in diffs if d == 0),
    }
