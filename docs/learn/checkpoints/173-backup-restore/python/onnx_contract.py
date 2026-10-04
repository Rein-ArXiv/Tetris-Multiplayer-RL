"""Validate the input/output boundary metadata of a tiny ONNX model.

validate_io() is a structural contract check only; the caller is expected to
run onnx.checker.check_model separately. This module never mutates the graph
and never tries to repair it.
"""

from __future__ import annotations

import onnx
from onnx import TensorProto


def _parse_specs(specs, role):
    """Return specs as a list of (name, dims) with dims a tuple of ints."""
    if not isinstance(specs, (list, tuple)):
        raise ValueError(f"{role} specs must be an ordered sequence of (name, dims) pairs")
    parsed = []
    for spec in specs:
        if not isinstance(spec, (tuple, list)) or len(spec) != 2:
            raise ValueError(f"{role} spec must be a (name, dims) pair")
        name, dims = spec
        if not isinstance(name, str) or not name:
            raise ValueError(f"{role} spec name must be a non-empty string")
        try:
            dims = tuple(dims)
        except TypeError:
            raise ValueError(f"{role} spec dims for {name!r} must be a sequence")
        for d in dims:
            if isinstance(d, bool) or not isinstance(d, int) or d <= 0:
                raise ValueError(
                    f"{role} spec {name!r} dimension {d!r} must be a positive integer"
                )
        parsed.append((name, dims))
    if len({name for name, _ in parsed}) != len(parsed):
        raise ValueError(f"{role} spec names must be unique")
    return parsed


def _type_name(elem_type):
    try:
        return TensorProto.DataType.Name(elem_type)
    except ValueError:
        return str(elem_type)


def _fixed_dims(value_info):
    """Return the tuple of dim_value dimensions, rejecting symbolic/unknown."""
    if not value_info.type.tensor_type.HasField("shape"):
        raise ValueError(f"{value_info.name!r} has unknown tensor rank")
    dims = []
    for i, dim in enumerate(value_info.type.tensor_type.shape.dim):
        if dim.HasField("dim_value"):
            dims.append(dim.dim_value)
        elif dim.HasField("dim_param") and dim.dim_param:
            raise ValueError(
                f"{value_info.name!r} dimension {i} is symbolic "
                f"({dim.dim_param!r}); only fixed dim_value dimensions are allowed"
            )
        else:
            raise ValueError(
                f"{value_info.name!r} dimension {i} has no fixed dim_value"
            )
    return tuple(dims)


def _check_value(value_info, name, dims, role):
    if value_info.name != name:
        raise ValueError(
            f"{role} name/order mismatch: expected {name!r}, found {value_info.name!r}"
        )
    if not value_info.type.HasField("tensor_type"):
        raise ValueError(f"{role} {name!r} is not a tensor")
    elem_type = value_info.type.tensor_type.elem_type
    if elem_type != TensorProto.FLOAT:
        raise ValueError(
            f"{role} {name!r} must be TensorProto.FLOAT, found {_type_name(elem_type)}"
        )
    actual = _fixed_dims(value_info)
    if len(actual) != len(dims):
        raise ValueError(
            f"{role} {name!r} rank mismatch: expected {len(dims)}, found {len(actual)}"
        )
    if actual != dims:
        raise ValueError(
            f"{role} {name!r} shape mismatch: expected {dims}, found {actual}"
        )


def validate_io(model_proto, input_specs, output_specs):
    """Validate graph input/output names, order, count, dtype and fixed shapes.

    input_specs / output_specs: ordered sequence of (name, dims) where dims is
    a tuple of positive, fixed integer dimensions.

    Raises ValueError on the first structural mismatch. Never modifies
    model_proto. The caller runs onnx.checker.check_model separately.
    """
    if not isinstance(model_proto, onnx.ModelProto):
        raise ValueError("model_proto must be an onnx.ModelProto")

    input_specs = _parse_specs(input_specs, "input")
    output_specs = _parse_specs(output_specs, "output")
    graph = model_proto.graph

    for role, values, specs in (
        ("input", graph.input, input_specs),
        ("output", graph.output, output_specs),
    ):
        if len(values) != len(specs):
            raise ValueError(
                f"graph has {len(values)} {role}(s), expected {len(specs)}"
            )
        for value_info, (name, dims) in zip(values, specs):
            _check_value(value_info, name, dims, role)

    initializer_names = {init.name for init in graph.initializer}
    for value_info in graph.input:
        if value_info.name in initializer_names:
            raise ValueError(
                f"graph input {value_info.name!r} is also an initializer; "
                "weights must be frozen in the graph, not passed as inputs"
            )
