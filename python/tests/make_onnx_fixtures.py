"""Generate tiny constant graphs for the C++ contract check, never trained policies.
Usage: uv run --no-project --with onnx python python/tests/make_onnx_fixtures.py /tmp/ort-check
Then: build-bot-polish/bot_onnx_contract_test /tmp/ort-check/valid.onnx /tmp/ort-check/invalid.onnx
"""
import sys
from pathlib import Path
import onnx
from onnx import helper, TensorProto
out = Path(sys.argv[1])
out.mkdir(parents=True, exist_ok=True)
for name, width in (("valid", 40), ("invalid", 41)):
    inputs = [helper.make_tensor_value_info(n, TensorProto.FLOAT, shape) for n, shape in
              (("board", [1, 1, 20, 10]), ("current", [1, 7]), ("next", [1, 7]))]
    outputs = [helper.make_tensor_value_info("policy_logits", TensorProto.FLOAT, [1, width]),
               helper.make_tensor_value_info("value", TensorProto.FLOAT, [1])]
    nodes = [helper.make_node("Constant", [], ["policy_logits"], value=helper.make_tensor("p", TensorProto.FLOAT, [1, width], list(range(width)))),
             helper.make_node("Constant", [], ["value"], value=helper.make_tensor("v", TensorProto.FLOAT, [1], [0]))]
    graph = helper.make_graph(nodes, "contract-test-only", inputs, outputs)
    model = helper.make_model(graph, opset_imports=[helper.make_opsetid("", 17)], ir_version=9)
    onnx.checker.check_model(model)
    onnx.save(model, out / (name + ".onnx"))
