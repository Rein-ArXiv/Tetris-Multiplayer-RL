"""Turn a saved CPU TrainingRun into a checked single-file inference graph."""
import argparse
import json
from pathlib import Path
import sys
from experiment import identity,hash_file


def collect_cases(model, seeds, pieces):
    import numpy as np
    import torch
    from gym_env import RoundEnv
    from ppo import batch_of
    from actions import masked_log_softmax
    from rewards import RewardSpec
    if (not seeds or len(set(seeds)) != len(seeds)
            or any(type(s) is not int or not 0 <= s <= 0xffffffff for s in seeds)):
        raise ValueError('distinct uint32 probe seeds required')
    if type(pieces) is not int or pieces <= 0:
        raise ValueError('probe pieces must be positive')
    cases=[]
    env=RoundEnv(reward_spec=RewardSpec(shaping_scale=0.))
    try:
        for seed in seeds:
            obs,info=env.reset(seed=seed)
            for _ in range(pieces):
                with torch.no_grad():
                    batch=batch_of(obs,'cpu')
                    logits,_=model(batch['board'],batch['current'],batch['next'])
                    mask=torch.as_tensor(info['legal_mask']).unsqueeze(0)
                    action=int(masked_log_softmax(logits,mask).argmax(-1)[0])
                cases.append(dict(inputs=[batch[key].numpy().copy() for key in ('board','current','next')],
                                  legal_mask=np.array(info['legal_mask'],copy=True)))
                obs,_,term,trunc,info=env.step(action)
                if term or trunc:break
    finally:
        env.close()
    return cases


def export_checkpoint(source, destination, *, module_dir, seeds, pieces, opset=17):
    source,destination=Path(source).resolve(),Path(destination).resolve()
    if source==destination:raise ValueError('source and destination cannot be the same')
    contract=identity(module_dir)
    sys.path.insert(0,str(Path(module_dir).resolve()))
    import study_py
    if str(Path(study_py.__file__).resolve())!=contract['module']:
        raise RuntimeError('unexpected native module')
    import torch
    from training_checkpoint import TrainingRun
    from onnx_pipeline import export_checked
    before=hash_file(source)
    with torch.random.fork_rng(devices=[]):
        run=TrainingRun.load(source)
        try:
            if hash_file(source)!=before:raise RuntimeError('source changed while loading')
            run.model.eval()
            schema=run.model.schema
            inputs=[('board',(1,1,schema.rows,schema.cols)),
                    ('current',(1,schema.pieces)),('next',(1,schema.pieces))]
            outputs=[('policy_logits',(1,schema.actions)),('value',(1,))]
            cases=collect_cases(run.model,list(seeds),pieces)
            if identity(module_dir)!=contract:raise RuntimeError('probe implementation changed')
            metadata={'study.policy':json.dumps(dict(version=1,observation=study_py.observation_schema(),
                       action=study_py.action_schema(),checkpoint_sha256=before),sort_keys=True)}
            evidence=export_checked(run.model,destination,inputs,outputs,cases,opset=opset,metadata=metadata)
            return dict(source_sha256=before,onnx_sha256=hash_file(destination),
                        native_sha256=contract['module_hash'],source_hash=contract['source'],
                        evidence=evidence)
        finally:
            run.close()


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('checkpoint');parser.add_argument('onnx')
    parser.add_argument('--module-dir',required=True);parser.add_argument('--seeds',type=int,nargs='+',required=True)
    parser.add_argument('--probe-pieces',type=int,required=True);parser.add_argument('--opset',type=int,default=17)
    args=parser.parse_args()
    result=export_checkpoint(args.checkpoint,args.onnx,module_dir=args.module_dir,
                             seeds=args.seeds,pieces=args.probe_pieces,opset=args.opset)
    print(json.dumps(result,indent=2,allow_nan=False))


if __name__=='__main__':main()
