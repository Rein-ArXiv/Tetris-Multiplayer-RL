import argparse
from pathlib import Path
import sys
import numpy as np
import torch
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/"python"))
from actions import encode,decode,legal_mask,masked_log_softmax,entropy


def require(value,message):
    if not value:raise AssertionError(message)


def rejected(call):
    try:call()
    except ValueError:return
    raise AssertionError("invalid action accepted")


def main():
    parser=argparse.ArgumentParser();parser.add_argument('--module-dir',required=True)
    args=parser.parse_args();sys.path.insert(0,args.module_dir)
    import study_py as sim
    schema=sim.action_schema()
    for action in range(schema['count']):
        require(encode(*decode(action,schema),schema)==action,'index inverse')
    for bad in [-1,schema['count'],2**63,1.0,True]:
        rejected(lambda:decode(bad,schema))
    matched=0
    for seed in range(1,9):
        game=sim.Session(seed,7)
        for _ in range(12):
            before=game.state_bytes()
            mask=legal_mask(game,schema)
            require(mask.dtype==np.bool_ and mask.shape==(schema['count'],),'mask contract')
            require(game.state_bytes()==before,'enumeration is read-only')
            for action in range(schema['count']):
                if not mask[action]:
                    rejected(lambda:game.apply_action(action))
                    require(game.state_bytes()==before,'rejection is atomic')
                    continue
                trace=game.action_trace(action)
                macro=game.clone();replay=game.clone()
                result=macro.apply_action(action)
                for i,frame in enumerate(trace):
                    step=replay.step(frame)
                    if i+1<len(trace):require(step in (sim.Step.waiting,sim.Step.changed),'no early lock')
                require(macro.state_bytes()==replay.state_bytes(),'macro and real ticks')
                require(result['ticks']==len(trace) and result['ended']==macro.finished(),'duration contract')
                matched+=1
            choices=np.flatnonzero(mask)
            if len(choices)==0:break
            game.apply_action(int(choices[seed%len(choices)]))
        for invalid in [-1,schema['count']]:
            before=game.state_bytes();rejected(lambda:game.apply_action(invalid))
            require(game.state_bytes()==before,'invalid label state')
    # Only a distribution with at least one finite legal logit is meaningful.
    x=torch.tensor([[0.,1.,2.]],requires_grad=True)
    mask=torch.tensor([[True,False,True]])
    logp=masked_log_softmax(x,mask)
    h=entropy(logp,mask);h.sum().backward()
    require(torch.isfinite(x.grad).all() and x.grad[0,1]==0,'finite masked gradient')
    require(logp.exp()[0,1]==0 and torch.allclose(logp.exp().sum(-1),torch.ones(1)),'normalized legal mass')
    rejected(lambda:masked_log_softmax(torch.zeros(2),torch.zeros(2,dtype=torch.bool)))
    limit=torch.finfo(torch.float32).max
    rejected(lambda:masked_log_softmax(torch.tensor([limit,-limit]),torch.tensor([True,True])))
    print('Action labels/masks/rejections and real-tick macro parity passed:',matched,'plans')
    print('Finite legal distribution and backward entropy passed')


if __name__=='__main__':main()
