"""Use one actual model with independent character appearance and pacing profiles."""
import argparse
from pathlib import Path
import shlex
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
        run.model.eval();cfg=folder/'characters.cfg'
        cfg.write_text(f'a|Alpha|{graph}|old.png|portrait.png|Calm|3|2|8\n'
                       f'b|Beta|{graph}|new.png|other.png|Label|3|2|8\n'
                       f'c|Gamma|{graph}|||Fast|1|0|1\n',encoding='utf-8')
        traces=[];total=0
        for identity in ('a','b','c'):
            result=subprocess.run([a.probe,str(cfg),identity,'91','180'],text=True,capture_output=True,timeout=60,check=True)
            lines=result.stdout.splitlines();card=shlex.split(lines.pop(0));assert card[:2]==['character',identity]
            assert card[2]=={'a':'Alpha','b':'Beta','c':'Gamma'}[identity]
            traces.append(lines);state=study_py.Session(91)
            for tick,row in enumerate(lines):
                fields=row.split();number,mask,event,attempted,selected,score,finished=map(int,fields[:7]);assert number==tick
                if attempted:
                    legal=state.legal_actions()
                    if legal:
                        batch=to_tensors(stack_batch([observe(state,study_py.observation_schema())]))
                        with torch.no_grad():logits,_=run.model(batch['board'],batch['current'],batch['next'])
                        assert selected==max(legal,key=lambda i:(float(logits[0,i]),-i))
                    else:assert selected==-1
                else:assert selected==-1
                state.step(mask);assert (state.score(),int(state.finished()))==(score,finished)
                assert state.state_bytes().hex()==fields[7];total+=1
            assert lines and (len(lines)==180 or state.finished())
        assert traces[0]==traces[1], 'appearance changed rules'
        assert traces[0]!=traces[2], 'fixture pacing did not change trace'
        print('Actual shared model: appearance-only traces equal, paced traces differ; Python/native frames matched:',total)
    finally:run.close()
