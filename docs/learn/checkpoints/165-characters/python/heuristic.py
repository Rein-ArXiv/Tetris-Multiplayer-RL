"""A deterministic one-decision baseline, with an explicit information budget."""
from dataclasses import dataclass, fields
import math
from heuristic_features import Features, extract
from versus_reward import finite_coefficient
from versus_env import VersusEnv


@dataclass(frozen=True)
class Weights:
    # Checkpoint experiment settings, not universal or learned coefficients.
    aggregate_height: float = -1.0
    bumpiness: float = -0.5
    holes: float = -4.0
    max_height: float = 0.0
    wells: float = 0.0
    lines: float = 10.0

    def __post_init__(self):
        for field in fields(self):
            value = finite_coefficient(getattr(self, field.name), field.name)
            object.__setattr__(self, field.name, value)


def evaluate(features: Features, lines: int, weights: Weights):
    if type(lines) is not int or lines < 0:
        raise ValueError('lines must be a nonnegative integer')
    try:
        value = sum(getattr(weights, name) * getattr(features, name)
                    for name in ('aggregate_height', 'bumpiness', 'holes', 'max_height', 'wells'))
        value += weights.lines * lines
    except OverflowError as exc:
        raise ValueError('evaluation overflow') from exc
    if not math.isfinite(value):
        raise ValueError('evaluation must be finite')
    return value


@dataclass(frozen=True)
class Decision:
    action: int
    score: float
    ended: bool
    candidates: int


class GreedyPolicy:
    def __init__(self, weights=Weights(), *, prefer_survival=True):
        if not isinstance(weights, Weights) or type(prefer_survival) is not bool:
            raise ValueError('Weights and a boolean survival policy required')
        self.weights = weights
        self.prefer_survival = prefer_survival

    def reseed(self, seed):
        pass  # No random sampling is used by this policy.

    def reset(self):
        pass

    def choose(self, session):
        best = None
        actions = sorted(session.legal_actions())
        for action in actions:
            candidate = session.clone()
            event = candidate.apply_action(action)
            features = extract(candidate.grid())
            score = evaluate(features, event['lines'], self.weights)
            alive = not candidate.finished()
            # Exact ties choose the smaller label, independent of enumeration order.
            key = (alive if self.prefer_survival else True, score, -action)
            if best is None or key > best[0]:
                best = key, Decision(action, score, not alive, len(actions))
        return None if best is None else best[1]


class PlanningVersusEnv(VersusEnv):
    """An explicit model-access adapter; observation-only opponents use VersusEnv."""
    def __init__(self, *, planner=None, **kwargs):
        if 'opponent' in kwargs:
            raise ValueError('supply planner to the model-access environment')
        super().__init__(opponent=planner if planner is not None else GreedyPolicy(), **kwargs)

    def _opponent_action(self, board, mask):
        decision = self._opponent.choose(board.clone())
        return None if decision is None else decision.action
