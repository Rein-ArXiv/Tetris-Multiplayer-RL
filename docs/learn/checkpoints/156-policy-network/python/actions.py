"""Action labels and their current-state mask; the C++ planner owns legality."""
import numpy as np


def integer(value):
    if isinstance(value,(bool,np.bool_)) or not isinstance(value,(int,np.integer)):
        raise ValueError("expected integer action coordinate")
    return int(value)


def dimensions(schema):
    if schema["version"] != 1:
        raise ValueError("unsupported action schema")
    columns,orientations=integer(schema["columns"]),integer(schema["orientations"])
    if columns <= 0 or orientations <= 0 or schema["count"] != columns*orientations:
        raise ValueError("inconsistent action dimensions")
    return columns,orientations


def encode(column,quarter,schema):
    columns,orientations=dimensions(schema)
    column,quarter=integer(column),integer(quarter)
    if not 0 <= column < columns or not 0 <= quarter < orientations:
        raise ValueError("target outside action domain")
    return column*orientations+quarter


def decode(action,schema):
    columns,orientations=dimensions(schema)
    action=integer(action)
    if not 0 <= action < columns*orientations:
        raise ValueError("action outside domain")
    return divmod(action,orientations)


def legal_mask(session,schema):
    columns,orientations=dimensions(schema)
    mask=np.zeros(columns*orientations,dtype=np.bool_)
    for action in session.legal_actions():
        decode(action,schema)  # Do not allow a negative Python index to alias the end.
        mask[action]=True
    return mask


def masked_log_softmax(logits,mask):
    import torch
    if not logits.is_floating_point() or mask.dtype != torch.bool:
        raise TypeError("floating logits and bool mask required")
    if logits.ndim < 1 or logits.shape[-1] == 0 or logits.shape != mask.shape:
        raise ValueError("matching nonempty action axis required")
    if logits.device != mask.device:
        raise ValueError("same device required")
    if not mask.any(dim=-1).all():
        raise ValueError("no legal action in a row")
    if not torch.isfinite(logits.masked_select(mask)).all():
        raise ValueError("legal logits must be finite")
    logp = torch.log_softmax(logits.masked_fill(~mask,float("-inf")),dim=-1)
    if not torch.isfinite(logp.masked_select(mask)).all():
        raise ValueError("normalized legal log probabilities must be finite")
    return logp


def entropy(logp,mask):
    # Mask the operand before multiplication so the backward graph avoids 0 * -inf.
    safe=logp.masked_fill(~mask,0.0)
    return -(logp.exp()*safe).sum(dim=-1)
