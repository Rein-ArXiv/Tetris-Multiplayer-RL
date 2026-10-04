"""Model weight round trips and explicit class/version compatibility.

This does not exercise a Colab machine or optimizer/environment resume.
"""

from __future__ import annotations

from pathlib import Path

import pytest

torch = pytest.importorskip("torch")

from common.checkpoint import (
    CHECKPOINT_META_KEY,
    load_checkpoint,
    save_checkpoint,
)
from common.models import TetrisPolicyNet


def _state_dict_equal(a: dict, b: dict) -> bool:
    if a.keys() != b.keys():
        return False
    return all(torch.equal(a[k], b[k]) for k in a)


def test_save_load_round_trip(tmp_path: Path) -> None:
    model = TetrisPolicyNet()
    path = tmp_path / "rt.pt"
    save_checkpoint(model, path, extra={"step": 1234})

    loaded = load_checkpoint(path, device="cpu")
    assert isinstance(loaded, TetrisPolicyNet)
    assert _state_dict_equal(model.state_dict(), loaded.state_dict())


def test_arch_version_mismatch_raises(tmp_path: Path) -> None:
    model = TetrisPolicyNet()
    path = tmp_path / "old.pt"

    # Hand-craft a payload with a stale arch_version.
    payload = {
        "state_dict": model.state_dict(),
        CHECKPOINT_META_KEY: {
            "arch_version": TetrisPolicyNet.ARCH_VERSION - 1,
            "class": "TetrisPolicyNet",
        },
    }
    torch.save(payload, str(path))

    with pytest.raises(RuntimeError, match="arch_version"):
        load_checkpoint(path)


def test_class_mismatch_raises(tmp_path: Path) -> None:
    model = TetrisPolicyNet()
    path = tmp_path / "wrong_class.pt"
    payload = {
        "state_dict": model.state_dict(),
        CHECKPOINT_META_KEY: {
            "arch_version": TetrisPolicyNet.ARCH_VERSION,
            "class": "SomethingElseNet",
        },
    }
    torch.save(payload, str(path))

    with pytest.raises(RuntimeError, match="class"):
        load_checkpoint(path)


def test_extra_metadata_does_not_break_load(tmp_path: Path) -> None:
    model = TetrisPolicyNet()
    path = tmp_path / "extra.pt"
    save_checkpoint(
        model,
        path,
        extra={
            "training_steps": 12345,
            "wandb_run_id": "abc123",
            "git_sha": "deadbeef",
        },
    )
    loaded = load_checkpoint(path)
    assert _state_dict_equal(model.state_dict(), loaded.state_dict())
