"""Real ONNX inference through paced two-board execution and strict replay."""
import argparse
from pathlib import Path
import subprocess
import sys
import tempfile
p=argparse.ArgumentParser();p.add_argument('--module-dir',required=True);p.add_argument('--probe',required=True)
a=p.parse_args();cp=Path(__file__).resolve().parents[1]
sys.path[:0]=[str(Path(a.module_dir).resolve()),str(cp/'python')]
import study_py
import torch
import onnx
import numpy as np
from onnx import helper,numpy_helper
from training_checkpoint import TrainingRun
from export_policy import export_checkpoint
from observation import observe,stack_batch,to_tensors

torch.set_num_threads(1)
with tempfile.TemporaryDirectory() as folder:
    folder=Path(folder);train=folder/'train.pt';graph=folder/'policy.onnx';bad=folder/'nan.onnx';run=TrainingRun()
    try:
        run.advance(3);run.save(train);export_checkpoint(train,graph,module_dir=a.module_dir,seeds=[91],pieces=3)
        run.model.eval();original=onnx.load(graph)
        failure=helper.make_graph([
            helper.make_node('Constant',[],['policy_logits'],value=numpy_helper.from_array(np.full((1,study_py.action_schema()['count']),np.nan,dtype=np.float32))),
            helper.make_node('Constant',[],['value'],value=numpy_helper.from_array(np.zeros((1,),dtype=np.float32)))
        ],'failure-fixture',list(original.graph.input),list(original.graph.output))
        fault=helper.make_model(failure,opset_imports=[helper.make_opsetid('',17)]);fault.ir_version=9;onnx.save(fault,bad)
        total=0
        for policy,is_bad in [(graph,False),(bad,True)]:
            for mode in ('practice','strict'):
                result=subprocess.run([a.probe,str(policy),'91','240',mode],text=True,capture_output=True,check=True,timeout=60)
                lines=result.stdout.splitlines();verdict=int(lines.pop().split()[1])
                human=study_py.Session(91);enemy=study_py.Session(91);h_sent=e_sent=0;failed=False
                for tick,line in enumerate(lines):
                    row=line.split();number,mask,source,eligible,selected,hscore,escore,finished=map(int,row[:8]);assert number==tick
                    legal=enemy.legal_actions()
                    if source==1: # primary
                        batch=to_tensors(stack_batch([observe(enemy,study_py.observation_schema())]))
                        with torch.no_grad():logits,_=run.model(batch['board'],batch['current'],batch['next'])
                        assert not is_bad and selected==max(legal,key=lambda i:(float(logits[0,i]),-i))
                    elif source in (2,4):assert is_bad and selected==-1;failed=True
                    else:assert selected==-1
                    assert bool(eligible)==(not failed)
                    human.step(0);enemy.step(mask)
                    h_total=human.attack_sent();e_total=enemy.attack_sent();cap=study_py.observation_schema()['rows']
                    if h_total>h_sent and not enemy.finished():enemy.add_garbage(min(h_total-h_sent,cap))
                    if e_total>e_sent and not human.finished():human.add_garbage(min(e_total-e_sent,cap))
                    h_sent,e_sent=h_total,e_total
                    assert (human.score(),enemy.score(),bool(human.finished() or enemy.finished()))==(hscore,escore,bool(finished))
                    assert human.state_bytes().hex()==row[8] and enemy.state_bytes().hex()==row[9];total+=1
                assert lines
                if is_bad:assert failed and verdict==2
                else:assert not failed and verdict==(0 if enemy.finished() and not human.finished() else 1)
        print('Actual model and NaN runtime: practice/strict provenance, eligibility, both native states, strict verdicts; frames:',total)
    finally:run.close()
