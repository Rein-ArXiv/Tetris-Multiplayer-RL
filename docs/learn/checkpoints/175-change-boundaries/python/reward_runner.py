"""Read-only transition preparation: commit the returned Session explicitly."""
from dataclasses import dataclass
from rewards import RewardSpec, RewardParts, evaluate


def board_potential(board):
    """Fixed bounded potential on a rectangular binary board: height and holes."""
    if not board or not board[0]:
        raise ValueError('nonempty board required')
    rows,cols=len(board),len(board[0])
    if any(len(row)!=cols for row in board):
        raise ValueError('rectangular board required')
    if any(type(cell) not in (int,bool) or cell not in (0,1)
           for row in board for cell in row):
        raise ValueError('binary integer cells required')
    height_sum=holes=0
    for col in range(cols):
        first=next((row for row in range(rows) if board[row][col]),rows)
        height_sum+=rows-first
        holes+=sum(board[row][col]==0 for row in range(first,rows))
    return -(height_sum/rows+holes)


@dataclass(frozen=True)
class Transition:
    reward: RewardParts
    lines: int
    ticks: int
    terminated: bool
    score_delta: int
    before_potential: float
    after_potential: float


def execute(session,action,spec: RewardSpec):
    if not isinstance(spec,RewardSpec):
        raise TypeError('spec must be RewardSpec')
    before=board_potential(session.grid())
    points=session.score()
    candidate=session.clone()
    result=candidate.apply_action(action)
    after=board_potential(candidate.grid())
    parts=evaluate(result['lines'],result['ticks'],result['ended'],before,after,spec)
    transition=Transition(parts,result['lines'],result['ticks'],result['ended'],
                          candidate.score()-points,before,after)
    # The caller commits only after both the rule transition and reward succeed.
    return candidate,transition
