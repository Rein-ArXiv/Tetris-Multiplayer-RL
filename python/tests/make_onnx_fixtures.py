"""Generate tiny constant graphs for the C++ contract check, never trained policies.
Usage: PYTHONPATH=python uv run --no-project --with onnx --with numpy python python/tests/make_onnx_fixtures.py /tmp/ort-check
Then pass valid.onnx, invalid.onnx, nan.onnx, inf.onnx, negative_inf.onnx,
and value_nan.onnx (in that order) to bot_onnx_contract_test.
"""
import sys
from pathlib import Path
import onnx
from common import BOARD_ROWS, BOARD_COLS, NUM_PIECE_TYPES, NUM_PLACEMENTS
from onnx import helper, TensorProto
out = Path(sys.argv[1])
out.mkdir(parents=True, exist_ok=True)
for name, width in (("valid", NUM_PLACEMENTS), ("invalid", NUM_PLACEMENTS + 1),
                    ("nan", NUM_PLACEMENTS), ("inf", NUM_PLACEMENTS),
                    ("negative_inf", NUM_PLACEMENTS), ("value_nan", NUM_PLACEMENTS)):
    inputs = [helper.make_tensor_value_info(n, TensorProto.FLOAT, shape) for n, shape in
              (("board", [1, 1, BOARD_ROWS, BOARD_COLS]), ("current", [1, NUM_PIECE_TYPES]), ("next", [1, NUM_PIECE_TYPES]))]
    outputs = [helper.make_tensor_value_info("policy_logits", TensorProto.FLOAT, [1, width]),
               helper.make_tensor_value_info("value", TensorProto.FLOAT, [1])]
    scores = list(range(width))
    if name in ('nan', 'inf', 'negative_inf'):
        scores[0] = float({'nan':'nan','inf':'inf','negative_inf':'-inf'}[name])
    value = float('nan') if name == 'value_nan' else 0.
    nodes = [helper.make_node("Constant", [], ["policy_logits"], value=helper.make_tensor("p", TensorProto.FLOAT, [1, width], scores)),
             helper.make_node("Constant", [], ["value"], value=helper.make_tensor("v", TensorProto.FLOAT, [1], [value]))]
    graph = helper.make_graph(nodes, "contract-test-only", inputs, outputs)
    model = helper.make_model(graph, opset_imports=[helper.make_opsetid("", 17)], ir_version=9)
    onnx.checker.check_model(model)
    onnx.save(model, out / (name + ".onnx"))
