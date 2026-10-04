"""Export a canonical checkpoint through the C++ bot's fixed I/O contract.

The deployment artifact is one ONNX file with embedded weights. Structure,
fixed float32 I/O and CPU Runtime outputs are checked before replacement.
Default probes exercise tensors; pass native observation cases for game-state
coverage. Neither probe set establishes playing strength or all-input parity.

Usage from python/: python -m netbot.export_onnx checkpoint.pt model.onnx
Install the export extra (PyTorch, ONNX and ONNX Runtime) in the export machine.
"""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path

import torch
from common import BOARD_COLS, BOARD_ROWS, NUM_PIECE_TYPES, NUM_PLACEMENTS
from common.checkpoint import load_checkpoint, IO_CONTRACT
from .onnx_pipeline import export_checked, tensor_probes

# Keep order/names consistent with BotOnnx's runtime boundary.
INPUT_SPECS=[('board',(1,1,BOARD_ROWS,BOARD_COLS)),
             ('current',(1,NUM_PIECE_TYPES)),('next',(1,NUM_PIECE_TYPES))]
OUTPUT_SPECS=[('policy_logits',(1,NUM_PLACEMENTS)),('value',(1,))]
INPUT_NAMES=[name for name,_ in INPUT_SPECS]
OUTPUT_NAMES=[name for name,_ in OUTPUT_SPECS]


def export(ckpt_path: str | Path, out_path: str | Path, opset: int=17, *, cases=None) -> dict:
    """Validate a private loaded model, then commit a checked single-file graph."""
    source,destination=Path(ckpt_path).resolve(),Path(out_path).resolve()
    if source == destination:
        raise ValueError('checkpoint and ONNX destination cannot be the same path')
    before=hashlib.sha256(source.read_bytes()).hexdigest()
    model=load_checkpoint(source,device='cpu')
    if hashlib.sha256(source.read_bytes()).hexdigest() != before:
        raise RuntimeError('checkpoint changed while loading')
    if (model.board_channels != 1 or model.n_piece_types != NUM_PIECE_TYPES
            or model.n_placements != NUM_PLACEMENTS):
        raise ValueError('model dimensions differ from the C++ deployment contract')
    metadata={'tetris.policy':json.dumps(dict(format_version=1,io_contract=IO_CONTRACT,
              checkpoint_sha256=before,arch_version=model.ARCH_VERSION),sort_keys=True),
              'tetris.exporter':json.dumps(dict(torch=str(torch.__version__),mode='torchscript',opset=opset))}
    evidence=export_checked(model,destination,INPUT_SPECS,OUTPUT_SPECS,
                            tensor_probes(INPUT_SPECS,NUM_PLACEMENTS) if cases is None else cases,
                            opset=opset,metadata=metadata)
    print('[export_onnx] wrote',destination,'from',source,json.dumps(evidence,allow_nan=False))
    return evidence


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('ckpt');parser.add_argument('out')
    parser.add_argument('--opset',type=int,default=17,help='ONNX operator-set revision; target Runtime must support it')
    args=parser.parse_args();export(args.ckpt,args.out,args.opset)


if __name__=='__main__':main()
