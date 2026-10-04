"""Export actual learned weights and native states; preserve old files on rejection."""
import argparse
from pathlib import Path
import json
import sys
import tempfile
import unittest
from unittest.mock import patch
p=argparse.ArgumentParser();p.add_argument('--module-dir',required=True);args,rest=p.parse_known_args()
CP=Path(__file__).resolve().parents[1];MODULE=Path(args.module_dir).resolve()
sys.path[:0]=[str(MODULE),str(CP/'python')]
import study_py
import torch
import onnx
import numpy as np
from training_checkpoint import TrainingRun
from export_policy import export_checkpoint,collect_cases
from onnx_contract import validate_io
from onnx_pipeline import ensure_embedded,compare_outputs,export_checked

torch.set_num_threads(1)


class Contract(unittest.TestCase):
    def test_actual_native_export_and_onnx_parity(self):
        with tempfile.TemporaryDirectory() as folder:
            folder=Path(folder);source=folder/'train.pt';target=folder/'policy.onnx'
            run=TrainingRun()
            try:run.advance(3);run.save(source)
            finally:run.close()
            before=torch.get_rng_state().clone()
            report=export_checkpoint(source,target,module_dir=MODULE,seeds=[41,93],pieces=3)
            self.assertEqual(report['evidence']['cases'],6)
            self.assertTrue(torch.equal(before,torch.get_rng_state()))
            graph=onnx.load(target);self.assertTrue(graph.graph.initializer)
            self.assertIn('study.policy',{p.key:p.value for p in graph.metadata_props})
            self.assertNotEqual(report['source_sha256'],report['onnx_sha256'])
            saved=target.read_bytes()
            with patch('onnx_pipeline.compare_outputs',side_effect=ValueError('injected numeric mismatch')):
                with self.assertRaisesRegex(ValueError,'injected'):
                    export_checkpoint(source,target,module_dir=MODULE,seeds=[41],pieces=2)
            self.assertEqual(target.read_bytes(),saved)
            with patch('onnx_pipeline.os.replace',side_effect=OSError('injected replace')):
                with self.assertRaisesRegex(OSError,'injected'):
                    export_checkpoint(source,target,module_dir=MODULE,seeds=[41],pieces=2)
            self.assertEqual(target.read_bytes(),saved)
            self.assertEqual(sorted(p.name for p in folder.iterdir()),['policy.onnx','train.pt'])

    def test_metadata_and_nested_external_weight(self):
        from onnx import helper,TensorProto
        def graph():
            return helper.make_model(helper.make_graph([], 'boundary',
                [helper.make_tensor_value_info('x',TensorProto.FLOAT,[1,2])],
                [helper.make_tensor_value_info('y',TensorProto.FLOAT,[1,2])]))
        for change in ('name','dtype','rank'):
            model=graph()
            if change=='name':model.graph.input[0].name='wrong'
            elif change=='dtype':model.graph.output[0].type.tensor_type.elem_type=TensorProto.DOUBLE
            else:model.graph.input[0].type.tensor_type.ClearField('shape')
            with self.assertRaises(ValueError):validate_io(model,[('x',(1,2))],[('y',(1,2))])
        weight=helper.make_tensor('weight',TensorProto.FLOAT,[1],[1.])
        weight.data_location=TensorProto.EXTERNAL
        model=graph();model.graph.node.append(helper.make_node('Constant',[],['c'],value=weight))
        with self.assertRaisesRegex(ValueError,'embedded'):ensure_embedded(model)

    def test_symbolic_tensor_is_not_fixed_contract(self):
        from onnx import helper,TensorProto
        g=helper.make_model(helper.make_graph([], 'io-only',
             [helper.make_tensor_value_info('x',TensorProto.FLOAT,['batch',2])],
             [helper.make_tensor_value_info('y',TensorProto.FLOAT,[1,2])]))
        with self.assertRaisesRegex(ValueError,'symbolic'):
            validate_io(g,[('x',(1,2))],[('y',(1,2))])

    def test_argmax_is_separate_from_numeric_tolerance(self):
        class Model(torch.nn.Module):
            def forward(self,x):return torch.tensor([[0.,1e-7]]),torch.tensor([0.])
        class Session:
            def run(self,*a):return [np.array([[1e-7,0.]],np.float32),np.array([0.],np.float32)]
        with patch('onnx_pipeline.ort.InferenceSession',return_value=Session()):
            with self.assertRaisesRegex(ValueError,'argmax'):
                compare_outputs(Model(),'unused',[dict(inputs=[np.zeros((1,1),np.float32)],
                                legal_mask=np.array([True,True]))],
                                [('x',(1,1))],[('policy_logits',(1,2)),('value',(1,))])


if __name__=='__main__':unittest.main(argv=[sys.argv[0],*rest])
