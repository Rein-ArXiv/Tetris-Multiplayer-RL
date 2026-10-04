import pytest

from train.evaluation_summary import paired_difference, summarize


def episode(seed, lines=0, score=0, pieces=0, reward=0.0, end='terminated'):
    return {'seed': seed, 'lines': lines, 'score': score,
            'pieces': pieces, 'reward': reward, 'end': end}


def test_summarize_basic():
    rows = [episode(1, lines=2, score=10, reward=1.5),
            episode(2, lines=4, score=20, reward=2.5, end='truncated')]
    out = summarize(rows)
    assert out['episodes'] == 2
    assert out['metrics']['lines'] == {
        'mean': 3, 'median': 3, 'minimum': 2, 'maximum': 4}
    assert out['endings'] == {'terminated': 1, 'truncated': 1, 'budget': 0}


@pytest.mark.parametrize('rows', [
    [],
    [episode(True)],
    [episode(1), episode(1)],
    [episode(1, lines=-1)],
    [episode(1, reward=float('nan'))],
    [episode(1, end='crashed')],
])
def test_summarize_rejects_invalid(rows):
    with pytest.raises(ValueError):
        summarize(rows)


def test_paired_difference_signed_and_order():
    left = [episode(1, lines=1), episode(2, lines=5), episode(3, lines=3)]
    right = [episode(1, lines=2), episode(2, lines=4), episode(3, lines=3)]
    out = paired_difference(left, right)
    assert out['minimum'] == -1
    assert out['maximum'] == 1
    assert out['positive_seeds'] == 1
    assert out['negative_seeds'] == 1
    assert out['tied_seeds'] == 1
    with pytest.raises(ValueError):
        paired_difference(left, [episode(2), episode(1), episode(3)])
