"""Eager policy metadata checks; callers own observation values and graph boundaries."""

import numbers

import torch


def positive_size(value, name):
    """Require a positive integral dimension; bool is not a model size."""
    if isinstance(value, bool) or not isinstance(value, numbers.Integral):
        raise TypeError(
            f'{name} must be an integer, got {type(value).__name__}'
        )
    if value <= 0:
        raise ValueError(f'{name} must be positive, got {value!r}')
    return int(value)


def validate_policy_inputs(
    board,
    current,
    next_piece,
    *,
    channels,
    rows,
    cols,
    pieces,
    parameter,
):
    """Check NCHW, piece widths, dtype and device at an eager call boundary.

    Dimensions have already been validated by the model constructor. Callers
    canonicalize layout first and skip this host check during tracing. Tensor
    values (binary occupancy, one-hot semantics, finiteness) belong to the
    observation/data boundary; this helper performs no reductions or casts.
    """
    if not isinstance(parameter, torch.Tensor):
        raise TypeError(
            f'parameter must be a torch.Tensor, got {type(parameter).__name__}'
        )

    if not isinstance(board, torch.Tensor):
        raise TypeError(
            f'board must be a torch.Tensor, got {type(board).__name__}'
        )
    if not isinstance(current, torch.Tensor):
        raise TypeError(
            f'current must be a torch.Tensor, got {type(current).__name__}'
        )
    if not isinstance(next_piece, torch.Tensor):
        raise TypeError(
            f'next_piece must be a torch.Tensor, got {type(next_piece).__name__}'
        )

    if board.dim() != 4:
        raise ValueError(f'board must be 4D NCHW, got {board.dim()} dimensions')
    batch = board.shape[0]
    if batch <= 0:
        raise ValueError('board must be nonempty (batch must be positive)')
    board_shape = tuple(board.shape)
    if board_shape != (batch, channels, rows, cols):
        raise ValueError(
            f'board must have shape (B, {channels}, {rows}, {cols}), got {board_shape}'
        )

    for name, tensor in (('current', current), ('next_piece', next_piece)):
        if tensor.dim() != 2:
            raise ValueError(
                f'{name} must be 2D (B, {pieces}), got {tensor.dim()} dimensions'
            )
        tensor_shape = tuple(tensor.shape)
        if tensor_shape != (batch, pieces):
            raise ValueError(
                f'{name} must have shape (B, {pieces}), got {tensor_shape}'
            )

    for name, tensor in (
        ('board', board),
        ('current', current),
        ('next_piece', next_piece),
    ):
        if not tensor.dtype.is_floating_point:
            raise TypeError(
                f'{name} must be floating point, got {tensor.dtype}'
            )
        if tensor.dtype != parameter.dtype:
            raise TypeError(
                f'{name} dtype {tensor.dtype} does not match parameter dtype {parameter.dtype}'
            )
        if tensor.device != parameter.device:
            raise ValueError(
                f'{name} device {tensor.device} does not match parameter device {parameter.device}'
            )

    return None
