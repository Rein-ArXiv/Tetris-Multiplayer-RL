'''Surface feature extraction for rectangular binary boards.'''

from dataclasses import dataclass
from numbers import Integral


@dataclass(frozen=True)
class Features:
    '''Immutable surface statistics of a binary board.'''

    heights: tuple[int, ...]
    aggregate_height: int
    bumpiness: int
    holes: int
    max_height: int
    wells: int


def _validated(board):
    '''Validate shape and values; return (grid, width) or raise ValueError.'''
    if isinstance(board, (str, bytes, bytearray)):
        raise ValueError('board must be a sequence of rows')
    try:
        rows = list(board)
    except TypeError:
        raise ValueError('board must be a sequence of rows') from None
    if not rows:
        raise ValueError('board must be nonempty')
    grid = []
    width = None
    for row in rows:
        if isinstance(row, (str, bytes, bytearray)):
            raise ValueError('each row must be a sequence of binary values')
        try:
            cells = list(row)
        except TypeError:
            raise ValueError('each row must be a sequence of binary values') from None
        if width is None:
            if not cells:
                raise ValueError('board must be nonempty')
            width = len(cells)
        elif len(cells) != width:
            raise ValueError('board must be rectangular')
        for value in cells:
            if not isinstance(value, Integral) or value not in (0, 1):
                raise ValueError('board values must be 0/1 or bool')
        grid.append(cells)
    return grid, width


def extract(board):
    '''Return Features for a board given top-to-bottom rows of 0/1 cells.'''
    grid, width = _validated(board)
    height = len(grid)

    heights = []
    holes = 0
    for col in range(width):
        top = None
        for row in range(height):
            if grid[row][col]:
                top = row
                break
        if top is None:
            heights.append(0)
            continue
        heights.append(height - top)
        for row in range(top + 1, height):
            if not grid[row][col]:
                holes += 1

    heights = tuple(heights)
    bumpiness = sum(abs(heights[i] - heights[i + 1]) for i in range(width - 1))

    wells = 0
    for col in range(width):
        left = heights[col - 1] if col > 0 else height
        right = heights[col + 1] if col + 1 < width else height
        depth = min(left, right) - heights[col]
        if depth > 0:
            wells += depth * (depth + 1) // 2

    return Features(
        heights=heights,
        aggregate_height=sum(heights),
        bumpiness=bumpiness,
        holes=holes,
        max_height=max(heights),
        wells=wells,
    )
