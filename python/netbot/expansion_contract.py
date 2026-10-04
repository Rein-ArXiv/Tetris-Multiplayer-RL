"""Bound command fields before subtraction and sequence allocation."""
import numbers


def validate_expansion(cur_col, cur_rot, tgt_col, tgt_rot, num_rotations,
                       columns, expected_rotations):
    raw = (cur_col, cur_rot, tgt_col, tgt_rot, num_rotations)
    for value in raw:
        if isinstance(value, bool) or not isinstance(value, numbers.Integral):
            raise TypeError('expansion fields must be Integral; bool is not accepted')
    if num_rotations != expected_rotations:
        raise ValueError('num_rotations must equal expected_rotations')
    if not -columns <= cur_col < columns:
        raise ValueError('current column outside [-columns, columns)')
    if not 0 <= tgt_col < columns:
        raise ValueError('target column outside [0, columns)')
    if not 0 <= cur_rot < num_rotations:
        raise ValueError('current rotation outside [0, num_rotations)')
    if not 0 <= tgt_rot < num_rotations:
        raise ValueError('target rotation outside [0, num_rotations)')
    return int(cur_col), int(cur_rot), int(tgt_col), int(tgt_rot), int(num_rotations)
