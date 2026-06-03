"""Convert a trained TetrisPolicyNet checkpoint to ONNX for the C++ netbot.

The C++ runtime uses onnxruntime (see ``bot/bot_onnx.cpp``) rather than libtorch
or a Python subprocess. Training/export can stay in Colab; deployment only
needs the exported ONNX file and the ONNX Runtime CPU bundle.

Input/output names are load-bearing: ``bot/bot_onnx.cpp`` looks them up by
string. If you rename one here, the C++ side must change in lockstep (and the
existing ``model/*.onnx`` / ``model/bots/*.onnx`` bundles must be re-exported).

Usage::

    uv run --directory python python -m netbot.export_onnx \\
        checkpoints/run42/step_2000000.pt \\
        ../model/bots/run42.onnx
"""

from __future__ import annotations

import argparse
from pathlib import Path

try:
    import torch
except ImportError as exc:  # pragma: no cover - depends on optional local env
    raise SystemExit(
        "export_onnx requires PyTorch. Run this in Colab, or install the "
        "optional export dependencies (`uv sync --extra export`)."
    ) from exc

from common import BOARD_COLS, BOARD_ROWS, NUM_PIECE_TYPES
from common.checkpoint import load_checkpoint
from common.models import TetrisPolicyNet


# Must match bot/bot_onnx.cpp's inputNames / outputNames arrays.
INPUT_NAMES = ["board", "current", "next"]
OUTPUT_NAMES = ["policy_logits", "value"]


def export(ckpt_path: str | Path, out_path: str | Path, opset: int = 17) -> None:
    """Load ``ckpt_path`` (a TetrisPolicyNet .pt) and write an ONNX graph to
    ``out_path``.

    Batch size is fixed at 1 — the C++ netbot only ever runs single-step
    inference on one SimGame at a time. If a training-side consumer ever needs
    batched ONNX inference, add ``dynamic_axes={"board": {0: "batch"}, ...}``.
    """
    ckpt_path = Path(ckpt_path)
    out_path = Path(out_path)
    out_path.parent.mkdir(parents=True, exist_ok=True)

    model = load_checkpoint(ckpt_path, device="cpu")
    model.eval()

    # Dummy inputs matching common.obs.build_observation shapes with a batch dim.
    dummy_board = torch.zeros(1, 1, BOARD_ROWS, BOARD_COLS, dtype=torch.float32)
    dummy_current = torch.zeros(1, NUM_PIECE_TYPES, dtype=torch.float32)
    dummy_next = torch.zeros(1, NUM_PIECE_TYPES, dtype=torch.float32)

    torch.onnx.export(
        model,
        (dummy_board, dummy_current, dummy_next),
        str(out_path),
        input_names=INPUT_NAMES,
        output_names=OUTPUT_NAMES,
        opset_version=opset,
        dynamic_axes=None,
        do_constant_folding=True,
    )
    print(f"[export_onnx] wrote {out_path} from {ckpt_path}")


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("ckpt", help="path to trained .pt checkpoint (TetrisPolicyNet)")
    ap.add_argument("out",  help="output .onnx path (e.g. ../model/bots/run42.onnx)")
    ap.add_argument("--opset", type=int, default=17, help="ONNX opset (default: 17)")
    args = ap.parse_args()
    export(args.ckpt, args.out, args.opset)


if __name__ == "__main__":
    main()
