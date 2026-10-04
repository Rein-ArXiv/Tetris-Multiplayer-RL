"""Real C++ CPU runtime against PyTorch and native states, not a mocked Session."""
import argparse
from pathlib import Path
import subprocess
import sys
import tempfile
p=argparse.ArgumentParser();p.add_argument('--module-dir',required=True);p.add_argument('--probe',required=True);p.add_argument('--contract',required=True)
a=p.parse_args();cp=Path(__file__).resolve().parents[1]
sys.path[:0]=[str(Path(a.module_dir).resolve()),str(cp/'python')]
import study_py
import numpy as np
import onnx
from onnx import helper,TensorProto
import torch
from training_checkpoint import TrainingRun
from export_policy import export_checkpoint
from observation import observe,stack_batch,to_tensors

def execute(args):
    done=subprocess.run(list(map(str,args)),text=True,capture_output=True,timeout=60)
    if done.returncode:raise RuntimeError(f'{args}: {done.returncode}\n{done.stdout}\n{done.stderr}')
    return done

def fixture(path,kind,schema):
    width=schema.actions+(kind=='shape')
    scores=np.arange(width,dtype=np.float32)
    if kind in ('nan','inf','negative_inf'):scores[0]={'nan':np.nan,'inf':np.inf,'negative_inf':-np.inf}[kind]
    value=np.nan if kind=='value_nan' else 0.
    inputs=[helper.make_tensor_value_info(n,TensorProto.FLOAT,s) for n,s in
            [('board',[1,1,schema.rows,schema.cols]),('current',[1,schema.pieces]),('next',[1,schema.pieces])]]
    outputs=[helper.make_tensor_value_info('policy_logits',TensorProto.FLOAT,[1,width]),helper.make_tensor_value_info('value',TensorProto.FLOAT,[1])]
    nodes=[helper.make_node('Constant',[],['policy_logits'],value=helper.make_tensor('p',TensorProto.FLOAT,[1,width],scores)),
           helper.make_node('Constant',[],['value'],value=helper.make_tensor('v',TensorProto.FLOAT,[1],[value]))]
    graph=helper.make_model(helper.make_graph(nodes,'failure-fixture',inputs,outputs),opset_imports=[helper.make_opsetid('',17)],ir_version=9)
    onnx.checker.check_model(graph);onnx.save(graph,path)

torch.set_num_threads(1)
with tempfile.TemporaryDirectory() as folder:
    folder=Path(folder);train=folder/'train.pt';graph=folder/'policy.onnx'
    run=TrainingRun()
    try:
        run.advance(3);run.save(train)
        export_checkpoint(train,graph,module_dir=a.module_dir,seeds=[91,753],pieces=4)
        run.model.eval();total=0;max_error=0.
        for seed in (91,753,1607):
            done=execute([a.probe,graph,seed,6]);print(done.stderr.strip())
            state=study_py.Session(seed);rows=done.stdout.splitlines()
            if len(rows)!=6:raise AssertionError('fixture ended too early')
            for row in rows:
                values=row.split();action=int(values[0]);actual=np.array(values[1:],dtype=np.float32)
                batch=to_tensors(stack_batch([observe(state,study_py.observation_schema())]))
                with torch.no_grad():logits,value=run.model(batch['board'],batch['current'],batch['next'])
                expected=np.r_[value.numpy().ravel(),logits.numpy().ravel()]
                np.testing.assert_allclose(actual,expected,rtol=1e-4,atol=1e-5)
                max_error=max(max_error,float(np.max(np.abs(actual-expected))))
                legal=state.legal_actions();chosen=max(legal,key=lambda i:(float(logits[0,i]),-i))
                if action!=chosen:raise AssertionError('C++/PyTorch legal argmax mismatch')
                state.apply_action(action);total+=1
        paths=[]
        for kind in ('shape','nan','inf','negative_inf','value_nan'):
            path=folder/(kind+'.onnx');fixture(path,kind,run.model.schema);paths.append(path)
        execute([a.contract,graph,*paths])
        print('C++ actual-state parity:',total,'max_abs_error:',max_error,'failure/output ownership contracts passed')
    finally:run.close()
