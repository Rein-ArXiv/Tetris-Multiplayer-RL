"""Versioned model weights for inference or warm starting a new training run.

Strict tensor loading checks keys and shapes. The explicit I/O revision tracks
meaning changes that keep those shapes. Optimizer/RNG/environment state is not
part of this format; metadata counters do not turn it into a training resume.
"""
from __future__ import annotations
from pathlib import Path
from typing import Any
import math
import torch
from .models import TetrisPolicyNet
from . import BOARD_ROWS, BOARD_COLS
from .atomic_save import atomic_torch_save

CHECKPOINT_META_KEY = "__meta__"
FORMAT_VERSION = 1
# Bump semantic versions when encodings/labels change without changing shape.
IO_CONTRACT = {"board_rows": BOARD_ROWS, "board_cols": BOARD_COLS,
               "observation_version": 1, "action_version": 1}
CONFIG_KEYS = {"board_channels", "conv_channels", "hidden", "n_placements", "n_piece_types"}
RESERVED = {"arch_version", "class", "format_version", "config", "io_contract"}


def model_config(model):
    return {key: list(getattr(model, key)) if key == "conv_channels" else getattr(model, key)
            for key in sorted(CONFIG_KEYS)}


def _plain_metadata(value):
    """Keep annotations readable by weights_only without adding custom globals."""
    if value is None or type(value) in (str, bool, int):
        return True
    if type(value) is float:
        return math.isfinite(value)
    if type(value) in (list, tuple):
        return all(_plain_metadata(v) for v in value)
    if type(value) is dict:
        return all(type(k) is str and _plain_metadata(v) for k, v in value.items())
    return False


def _check_weights(state_dict):
    if not isinstance(state_dict, dict) or not state_dict:
        raise RuntimeError("Checkpoint state_dict is not a nonempty mapping.")
    for name, tensor in state_dict.items():
        if (type(name) is not str or not isinstance(tensor, torch.Tensor)
                or tensor.layout != torch.strided or tensor.dtype != torch.float32
                or not torch.isfinite(tensor).all()):
            raise RuntimeError("Checkpoint weights must be finite dense float32 tensors.")


def save_checkpoint(model: TetrisPolicyNet, path: str | Path,
                    extra: dict[str, Any] | None = None) -> None:
    """Synchronous single-writer save; do not update the model during this call."""
    if type(model) is not TetrisPolicyNet:
        raise TypeError("Only the canonical TetrisPolicyNet can use this format")
    extra = {} if extra is None else extra
    if type(extra) is not dict or not _plain_metadata(extra):
        raise TypeError("extra must contain plain finite metadata")
    if RESERVED.intersection(extra):
        raise ValueError("extra contains reserved checkpoint metadata")
    state = model.state_dict()
    _check_weights(state)
    payload = {"state_dict": state, CHECKPOINT_META_KEY: {
        **extra, "format_version": FORMAT_VERSION,
        "arch_version": TetrisPolicyNet.ARCH_VERSION, "class": "TetrisPolicyNet",
        "config": model_config(model), "io_contract": dict(IO_CONTRACT),
    }}
    atomic_torch_save(payload, path)


def load_checkpoint(path: str | Path, device: str | torch.device = "cpu") -> TetrisPolicyNet:
    """Validate on CPU, build a candidate, then return an eval-mode model.

    Files without format_version are the historical default-configuration format.
    weights_only limits unpickling; it is not an authenticity or resource-limit check.
    """
    payload = torch.load(str(path), map_location="cpu", weights_only=True)
    if not isinstance(payload, dict) or "state_dict" not in payload:
        raise RuntimeError("Checkpoint does not contain a TetrisPolicyNet state_dict.")
    meta = payload.get(CHECKPOINT_META_KEY)
    if not isinstance(meta, dict):
        raise RuntimeError("Checkpoint metadata is not a mapping.")
    recorded = meta.get("arch_version")
    if type(recorded) is not int or recorded != TetrisPolicyNet.ARCH_VERSION:
        raise RuntimeError(f"Checkpoint arch_version {recorded!r} is incompatible.")
    if meta.get("class") != "TetrisPolicyNet":
        raise RuntimeError("Checkpoint class must be TetrisPolicyNet.")
    config = {}
    if "format_version" in meta:
        version = meta["format_version"]
        if type(version) is not int or version != FORMAT_VERSION:
            raise RuntimeError("Unsupported checkpoint format_version.")
        contract = meta.get("io_contract")
        if (not isinstance(contract, dict) or set(contract) != set(IO_CONTRACT)
                or any(type(contract[k]) is not int or contract[k] != v for k,v in IO_CONTRACT.items())):
            raise RuntimeError("Checkpoint io_contract differs from this runtime.")
        config = meta.get("config")
        if not isinstance(config, dict) or set(config) != CONFIG_KEYS:
            raise RuntimeError("Checkpoint config is incomplete or contains unknown fields.")
    elif "config" in meta or "io_contract" in meta:
        raise RuntimeError("Checkpoint config requires format_version.")
    _check_weights(payload["state_dict"])
    try:
        model = TetrisPolicyNet(**config)
    except (TypeError, ValueError) as exc:
        raise RuntimeError("Invalid checkpoint config") from exc
    model.load_state_dict(payload["state_dict"], strict=True)
    return model.to(device).eval()
