"""Schema 1: locked board CHW, current/first-preview one-hot; not full state."""
import numpy as np

FIELDS = ("board", "current", "next")


def integer(value):
    if isinstance(value, (bool, np.bool_)) or not isinstance(value, (int, np.integer)):
        raise ValueError("expected an integer, not a converted scalar")
    return int(value)


def one_hot(order, size):
    order, size = integer(order), integer(size)
    if not 0 <= order < size:
        raise ValueError("one-hot index outside vocabulary")
    vec = np.zeros(size, dtype=np.float32)
    vec[order] = 1.0
    return vec


def encode(snapshot, rows, cols, piece_ids):
    rows, cols = integer(rows), integer(cols)
    ids = tuple(integer(pid) for pid in piece_ids)
    if not ids or len(set(ids)) != len(ids):
        raise ValueError("piece vocabulary must be nonempty and unique")
    if rows <= 0 or cols <= 0:
        raise ValueError("rows and cols must be positive")
    order_of = {pid: i for i, pid in enumerate(ids)}
    # Check the original values; casting first could round a bad value to 0 or 1.
    raw = np.asarray(snapshot["board"])
    if raw.shape != (rows, cols):
        raise ValueError("board must have shape (rows, cols)")
    if raw.dtype.kind not in "biuf" or not np.all(np.isfinite(raw)):
        raise ValueError("board must contain finite real numbers")
    if not np.all((raw == 0) | (raw == 1)):
        raise ValueError("board cells must be binary")
    board = np.array(raw, dtype=np.float32, order="C", copy=True)[None, :, :]
    current = integer(snapshot["current_id"])
    nxt = integer(snapshot["next_id"])
    if current not in order_of or nxt not in order_of:
        raise ValueError("unknown piece id")
    return {
        "board": board,
        "current": one_hot(order_of[current], len(ids)),
        "next": one_hot(order_of[nxt], len(ids)),
    }


def observe(session, schema):
    if schema["version"] != 1:
        raise ValueError("unsupported observation schema")
    return encode(session.snapshot(), schema["rows"], schema["cols"], schema["piece_ids"])


def stack_batch(observations):
    """Batch encode() outputs with the same schema; input arrays remain independent."""
    obs = list(observations)
    if not obs or any(set(o) != set(FIELDS) for o in obs):
        raise ValueError("batch must contain observations with the schema keys")
    if any(o[k].dtype != np.float32 for o in obs for k in FIELDS):
        raise ValueError("batch inputs must be float32")
    return {k: np.ascontiguousarray(np.stack([o[k] for o in obs], axis=0)) for k in FIELDS}


def to_tensors(arrays):
    """CPU tensors share these owned NumPy arrays; no copy and no GPU upload."""
    import torch
    return {k: torch.from_numpy(v) for k, v in arrays.items()}
