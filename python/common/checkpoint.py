"""Save / load wrappers with arch-version guarding.

The single point of failure for the Colab-train -> local-deploy workflow is a
silent architecture change: someone bumps a layer size in ``models.py``,
forgets to bump ``ARCH_VERSION``, retrains in Colab, downloads the .pt file,
and the netbot loads it (because the keys happen to align) and plays a
confused policy.

The save/load helpers here:

1. Embed an ``arch_version`` and the model class name in every checkpoint
2. Refuse to load a checkpoint whose recorded version differs from the current
   ``TetrisPolicyNet.ARCH_VERSION`` — fail loud, never silent

Bump ``TetrisPolicyNet.ARCH_VERSION`` whenever you change the network
shape or layer order.
"""

from __future__ import annotations

from pathlib import Path
from typing import Any

import torch

from .models import TetrisPolicyNet

CHECKPOINT_META_KEY = "__meta__"


def save_checkpoint(
    model: TetrisPolicyNet,
    path: str | Path,
    extra: dict[str, Any] | None = None,
) -> None:
    """Save a model state_dict together with arch version metadata.

    ``extra`` is merged into the metadata dict — use it for things like
    optimizer step count, replay buffer hash, training run id. None of those
    affect loading; they're for debugging.
    """
    payload = {
        "state_dict": model.state_dict(),
        CHECKPOINT_META_KEY: {
            "arch_version": TetrisPolicyNet.ARCH_VERSION,
            "class": "TetrisPolicyNet",
            **(extra or {}),
        },
    }
    torch.save(payload, str(path))


def load_checkpoint(
    path: str | Path,
    device: str | torch.device = "cpu",
) -> TetrisPolicyNet:
    """Load a checkpoint, raising ``RuntimeError`` on arch-version mismatch."""
    payload = torch.load(str(path), map_location=device, weights_only=False)
    meta = payload.get(CHECKPOINT_META_KEY, {})
    recorded = meta.get("arch_version")
    if recorded != TetrisPolicyNet.ARCH_VERSION:
        raise RuntimeError(
            f"Checkpoint arch_version {recorded!r} does not match current "
            f"TetrisPolicyNet.ARCH_VERSION {TetrisPolicyNet.ARCH_VERSION!r}. "
            "Either retrain with the new architecture or roll common/models.py "
            "back to the version this checkpoint was trained against."
        )
    if meta.get("class") != "TetrisPolicyNet":
        raise RuntimeError(
            f"Checkpoint class {meta.get('class')!r} != 'TetrisPolicyNet'. "
            "This loader only handles the canonical policy network."
        )

    model = TetrisPolicyNet()
    model.load_state_dict(payload["state_dict"])
    model.to(device).eval()
    return model
