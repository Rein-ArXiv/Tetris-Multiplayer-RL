"""Single-file ONNX export: inspect, execute, compare, then replace.

The caller supplies a CPU model, fixed tensor specs and representative cases.
Numeric checks cover those cases only. This is a deployment validation tool,
not a sandbox for untrusted graphs or a proof for every possible observation.
"""
import inspect
import os
from pathlib import Path
import tempfile

import numpy as np
import onnx
import onnxruntime as ort
import torch
from google.protobuf.message import Message
from onnx_contract import validate_io


def ensure_embedded(message):
    """Walk protobuf messages so nested tensors cannot hide external weights."""
    if isinstance(message, onnx.TensorProto):
        if message.data_location == onnx.TensorProto.EXTERNAL or message.external_data:
            raise ValueError('deployment requires embedded tensor data in one ONNX file')
    for descriptor, value in message.ListFields():
        if descriptor.type == descriptor.TYPE_MESSAGE:
            children = (value,) if isinstance(value,Message) else value
            for child in children:
                ensure_embedded(child)


def tensor_probes(input_specs, actions):
    """Synthetic tensor probes, deliberately separate from native game states."""
    rng = np.random.default_rng(42)
    cases = []
    for occupied in (False, True):
        values = [np.zeros(shape, np.float32) for _,shape in input_specs]
        if occupied:
            values[0][:] = rng.integers(0,2,size=values[0].shape)
        for piece in values[1:]:
            piece[0, int(rng.integers(piece.shape[-1]))] = 1.
        cases.append(dict(inputs=values, legal_mask=np.ones(actions,dtype=bool)))
    return cases


def validate_cases(cases, input_specs, actions):
    if not isinstance(cases,(list,tuple)) or not cases:
        raise ValueError('at least one verification case required')
    result=[]
    for case in cases:
        values=case['inputs']
        if len(values) != len(input_specs):
            raise ValueError('verification input count mismatch')
        owned=[]
        for value,(name,shape) in zip(values,input_specs,strict=True):
            array=np.asarray(value)
            if array.dtype != np.float32 or array.shape != tuple(shape) or not np.isfinite(array).all():
                raise ValueError('invalid verification tensor: '+name)
            owned.append(np.array(array,copy=True,order='C'))
        mask=np.asarray(case['legal_mask'])
        if mask.dtype != np.bool_ or mask.shape != (actions,) or not mask.any():
            raise ValueError('verification needs a nonempty legal mask')
        result.append(dict(inputs=owned,legal_mask=mask.copy()))
    return result


@torch.no_grad()
def compare_outputs(model, path, cases, input_specs, output_specs, *, rtol=1e-4, atol=1e-5):
    options=ort.SessionOptions()
    options.intra_op_num_threads=1
    session=ort.InferenceSession(str(path),sess_options=options,providers=['CPUExecutionProvider'])
    max_error=0.
    for case in cases:
        inputs=case['inputs']
        expected=model(*(torch.from_numpy(array) for array in inputs))
        actual=session.run([name for name,_ in output_specs],
                           {name:array for (name,_),array in zip(input_specs,inputs,strict=True)})
        if len(expected) != len(output_specs) or len(actual) != len(output_specs):
            raise ValueError('runtime output count mismatch')
        for reference,got,(name,shape) in zip(expected,actual,output_specs,strict=True):
            reference=reference.detach().cpu().numpy()
            if (reference.dtype != np.float32 or got.dtype != np.float32
                    or reference.shape != tuple(shape) or got.shape != tuple(shape)
                    or not np.isfinite(reference).all() or not np.isfinite(got).all()):
                raise ValueError('invalid runtime output: '+name)
            np.testing.assert_allclose(got,reference,rtol=rtol,atol=atol,err_msg=name)
            max_error=max(max_error,float(np.max(np.abs(got-reference))))
        legal=case['legal_mask']
        expected_action=int(np.argmax(np.where(legal,expected[0].detach().cpu().numpy()[0],-np.inf)))
        actual_action=int(np.argmax(np.where(legal,actual[0][0],-np.inf)))
        if actual_action != expected_action:
            raise ValueError('legal argmax changed despite numeric tolerance')
    return dict(cases=len(cases),max_abs_error=max_error,rtol=rtol,atol=atol,provider='CPUExecutionProvider')


def export_checked(model, out_path, input_specs, output_specs, cases, *, opset=17, metadata=None):
    """Single writer, fixed CPU/eval model; return evidence for this finite case set."""
    if type(opset) is not int or opset <= 0:
        raise ValueError('opset must be a positive integer')
    if any(p.device.type != 'cpu' or p.dtype != torch.float32 for p in model.parameters()):
        raise ValueError('export model must use CPU float32 parameters')
    cases=validate_cases(cases,input_specs,output_specs[0][1][-1])
    out=Path(out_path)
    out.parent.mkdir(parents=True,exist_ok=True)
    was_training=model.training
    try:
        model.eval()
        with tempfile.TemporaryDirectory(dir=out.parent,prefix='.'+out.name+'.') as folder:
            candidate=Path(folder)/'candidate.onnx'
            kwargs=dict(input_names=[n for n,_ in input_specs],output_names=[n for n,_ in output_specs],
                        opset_version=opset,export_params=True,keep_initializers_as_inputs=False,
                        dynamic_axes=None,do_constant_folding=True,training=torch.onnx.TrainingMode.EVAL)
            parameters=inspect.signature(torch.onnx.export).parameters
            if 'dynamo' in parameters:kwargs['dynamo']=False
            if 'external_data' in parameters:kwargs['external_data']=False
            torch.onnx.export(model,tuple(torch.from_numpy(a) for a in cases[0]['inputs']),str(candidate),**kwargs)
            graph=onnx.load(str(candidate),load_external_data=False)
            ensure_embedded(graph)
            validate_io(graph,input_specs,output_specs)
            imports={item.domain:item.version for item in graph.opset_import}
            if imports.get('') != opset:
                raise ValueError('exported opset differs from request')
            if metadata:
                onnx.helper.set_model_props(graph,metadata)
            onnx.checker.check_model(graph,full_check=True)
            onnx.save_model(graph,str(candidate),save_as_external_data=False)
            evidence=compare_outputs(model,candidate,cases,input_specs,output_specs)
            if {p.name for p in Path(folder).iterdir()} != {'candidate.onnx'}:
                raise ValueError('export created unexpected companion files')
            with candidate.open('r+b') as stream:
                stream.flush();os.fsync(stream.fileno())
            os.replace(candidate,out)
        return evidence
    finally:
        model.train(was_training)
