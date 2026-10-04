"""Deployment export must preserve the old artifact until all checks pass."""
from pathlib import Path
import pytest
torch = pytest.importorskip("torch")
pytest.importorskip("onnx")
pytest.importorskip("onnxruntime")
from common import NUM_PLACEMENTS
from common.models import TetrisPolicyNet
from common.checkpoint import save_checkpoint
from netbot import export_onnx as exporter

torch.set_num_threads(1)


def checkpoint(tmp_path,**config):
    path=tmp_path/'model.pt'
    save_checkpoint(TetrisPolicyNet(conv_channels=(2,),hidden=4,**config),path)
    return path


def test_exporter_failure_keeps_previous_file(tmp_path,monkeypatch):
    source=checkpoint(tmp_path);dest=tmp_path/'model.onnx';dest.write_bytes(b'previous model')
    def fail(model,inputs,path,**kw):
        Path(path).write_bytes(b'partial');raise RuntimeError('injected exporter failure')
    monkeypatch.setattr(torch.onnx,'export',fail)
    with pytest.raises(RuntimeError,match='injected'):
        exporter.export(source,dest)
    assert dest.read_bytes()==b'previous model'


def test_mismatched_action_contract_is_rejected_before_export(tmp_path,monkeypatch):
    source=checkpoint(tmp_path,n_placements=NUM_PLACEMENTS+1)
    def unexpected(*a,**k):pytest.fail('incompatible model reached exporter')
    monkeypatch.setattr(torch.onnx,'export',unexpected)
    with pytest.raises(ValueError,match='contract'):
        exporter.export(source,tmp_path/'model.onnx')


def test_source_and_destination_cannot_be_same(tmp_path):
    source=checkpoint(tmp_path);before=source.read_bytes()
    with pytest.raises(ValueError,match='same'):
        exporter.export(source,source)
    assert source.read_bytes()==before


def test_real_export_io_numerics_and_metadata(tmp_path):
    import onnx
    import numpy as np
    import onnxruntime as ort
    from netbot.onnx_contract import validate_io
    source=checkpoint(tmp_path);dest=tmp_path/'model.onnx'
    evidence=exporter.export(source,dest)
    assert evidence['cases']>0 and evidence['max_abs_error']<1e-4
    graph=onnx.load(dest);validate_io(graph,exporter.INPUT_SPECS,exporter.OUTPUT_SPECS)
    metadata={p.key:p.value for p in graph.metadata_props}
    import json,hashlib
    assert json.loads(metadata['tetris.policy'])['checkpoint_sha256']==hashlib.sha256(source.read_bytes()).hexdigest()
    session=ort.InferenceSession(str(dest),providers=['CPUExecutionProvider'])
    feeds={name:np.zeros(shape,np.float32) for name,shape in exporter.INPUT_SPECS}
    feeds['board']=np.zeros((2,*feeds['board'].shape[1:]),np.float32)
    with pytest.raises(Exception):session.run(None,feeds)


@pytest.mark.parametrize('stage',['checker','numerics','replace'])
def test_failed_admission_preserves_old_artifact(tmp_path,monkeypatch,stage):
    from netbot import onnx_pipeline as pipe
    source=checkpoint(tmp_path);dest=tmp_path/'model.onnx';dest.write_bytes(b'old')
    def fail(*a,**k):raise RuntimeError('injected '+stage)
    if stage=='checker':monkeypatch.setattr(pipe.onnx.checker,'check_model',fail)
    elif stage=='numerics':monkeypatch.setattr(pipe,'compare_outputs',fail)
    else:monkeypatch.setattr(pipe.os,'replace',fail)
    with pytest.raises(RuntimeError,match='injected'):
        exporter.export(source,dest)
    assert dest.read_bytes()==b'old'
    assert sorted(p.name for p in tmp_path.iterdir())==['model.onnx','model.pt']


def test_io_guard_rejects_symbolic_order_dtype_and_initializer():
    import onnx
    from onnx import helper,TensorProto
    from netbot.onnx_contract import validate_io
    specs=[('x',(1,2))];outs=[('y',(1,2))]
    def model():
        return helper.make_model(helper.make_graph([], 'boundary',
           [helper.make_tensor_value_info('x',TensorProto.FLOAT,[1,2])],
           [helper.make_tensor_value_info('y',TensorProto.FLOAT,[1,2])]))
    validate_io(model(),specs,outs)
    for kind in ('name','symbolic','dtype','initializer','count','unknown_rank'):
        graph=model()
        if kind=='name':graph.graph.input[0].name='other'
        elif kind=='symbolic':graph.graph.input[0].type.tensor_type.shape.dim[0].dim_param='batch'
        elif kind=='dtype':graph.graph.output[0].type.tensor_type.elem_type=TensorProto.DOUBLE
        elif kind=='unknown_rank':graph.graph.input[0].type.tensor_type.ClearField('shape')
        elif kind=='initializer':graph.graph.initializer.append(helper.make_tensor('x',TensorProto.FLOAT,[1,2],[0.,0.]))
        else:graph.graph.input.append(helper.make_tensor_value_info("extra",TensorProto.FLOAT,[1,2]))
        with pytest.raises(ValueError):validate_io(graph,specs,outs)


def test_nested_external_tensor_is_rejected():
    from onnx import helper,TensorProto
    from netbot.onnx_pipeline import ensure_embedded
    weight=helper.make_tensor('w',TensorProto.FLOAT,[1],[1.])
    weight.data_location=TensorProto.EXTERNAL
    model=helper.make_model(helper.make_graph([helper.make_node('Constant',[],['y'],value=weight)],'g',[],[]))
    with pytest.raises(ValueError,match='embedded'):ensure_embedded(model)


def test_close_logits_can_still_change_legal_action(tmp_path,monkeypatch):
    import numpy as np
    from netbot import onnx_pipeline as pipe
    class Model(torch.nn.Module):
        def forward(self,x):return torch.tensor([[0.,1e-7]]),torch.tensor([0.])
    class Session:
        def run(self,*a):return [np.array([[1e-7,0.]],np.float32),np.array([0.],np.float32)]
    monkeypatch.setattr(pipe.ort,'InferenceSession',lambda *a,**k:Session())
    case=dict(inputs=[np.zeros((1,1),np.float32)],legal_mask=np.array([True,True]))
    with pytest.raises(ValueError,match='argmax'):
        pipe.compare_outputs(Model(),tmp_path/'unused.onnx',[case],[('x',(1,1))],[('logits',(1,2)),('value',(1,))])
