"""Actual ONNX decisions are interleaved with paced empty ticks and native gravity."""
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
from training_checkpoint import TrainingRun
from export_policy import export_checkpoint
from observation import observe,stack_batch,to_tensors

torch.set_num_threads(1)
with tempfile.TemporaryDirectory() as folder:
    folder=Path(folder);train=folder/'train.pt';graph=folder/'policy.onnx';run=TrainingRun()
    try:
        run.advance(3);run.save(train);export_checkpoint(train,graph,module_dir=a.module_dir,seeds=[91],pieces=3)
        run.model.eval();total=decisions=0
        for seed in (91,712):
            for interval,think,minimum in ((1,0,1),(3,2,8),(8,12,50)):
                command=[a.probe,str(graph),str(seed),'180',str(interval),str(think),str(minimum)]
                done=subprocess.run(command,text=True,capture_output=True,timeout=60,check=True)
                again=subprocess.run(command,text=True,capture_output=True,timeout=60,check=True)
                if done.stdout!=again.stdout:raise AssertionError('paced replay differs')
                state=study_py.Session(seed);n=0
                for row in done.stdout.splitlines():
                    fields=row.split();tick,mask,event,attempted,selected,score,finished=map(int,fields[:7]);assert tick==n
                    if attempted:
                        legal=state.legal_actions()
                        if legal:
                            batch=to_tensors(stack_batch([observe(state,study_py.observation_schema())]))
                            with torch.no_grad():logits,_=run.model(batch['board'],batch['current'],batch['next'])
                            expected=max(legal,key=lambda i:(float(logits[0,i]),-i));assert selected==expected
                        else:assert selected==-1
                        decisions+=1
                    else:assert selected==-1
                    state.step(mask);assert (state.score(),int(state.finished()))==(score,finished)
                    assert state.state_bytes().hex()==fields[7]
                    n+=1;total+=1
                assert n>0 and (n==180 or state.finished())
        assert decisions>0
        print('Actual model paced frames:',total,'decisions:',decisions,'Python actions, native state bytes and repeated C++ trace matched')
    finally:run.close()
