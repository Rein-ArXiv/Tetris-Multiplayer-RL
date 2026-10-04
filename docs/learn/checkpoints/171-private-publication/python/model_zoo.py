"""Compare saved CPU TrainingRun candidates under one placement protocol."""
import argparse
import json
from pathlib import Path
import sys
from experiment import hash_file, identity, write_once
from evaluation_summary import summarize, paired_difference


def compare(paths, *, seeds, limit, module_dir, split):
    if split not in ('validation','test'):
        raise ValueError('choose validation or test')
    seeds=list(seeds)
    if not seeds or any(type(s) is not int or not 0 <= s <= 0xffffffff for s in seeds) or len(set(seeds))!=len(seeds):
        raise ValueError('use distinct uint32 seeds')
    if type(limit) is not int or limit <= 0:
        raise ValueError('limit must be positive')
    paths=[Path(p).resolve() for p in paths]
    if not paths or len(paths)!=len(set(paths)):
        raise ValueError('choose distinct candidate files')
    contract=identity(module_dir)
    sys.path.insert(0,str(Path(module_dir).resolve()))
    import study_py
    if str(Path(study_py.__file__).resolve())!=contract['module']:
        raise RuntimeError('unexpected native module loaded')
    import torch
    from training_checkpoint import TrainingRun
    from gym_env import RoundEnv
    from rewards import RewardSpec
    from ppo import evaluate
    records=[]
    # Loading restores the training RNG; keep it local to this comparison.
    with torch.random.fork_rng(devices=[]):
        for path in paths:
            artifact_hash=hash_file(path)
            run=TrainingRun.load(path)
            try:
                if hash_file(path)!=artifact_hash:
                    raise RuntimeError('candidate changed while loading')
                raw=evaluate(run.model,lambda:RoundEnv(reward_spec=RewardSpec(shaping_scale=0.)),
                             seeds=seeds,limit=limit,device='cpu')
                rows=[dict(seed=r['seed'],lines=r['lines'],score=r['score'],pieces=r['decisions'],
                           reward=r['reward'],end=('terminated' if r['terminated'] else
                                   'truncated' if r['truncated'] else 'budget')) for r in raw]
                records.append(dict(path=str(path),sha256=artifact_hash,
                                    training_decisions=run.decisions,training_updates=run.updates,
                                    rows=rows,summary=summarize(rows)))
            finally:
                run.close()
    if identity(module_dir)!=contract:
        raise RuntimeError('evaluation source or extension changed during comparison')
    return dict(format_version=1,contract=contract,
                protocol=dict(environment='RoundEnv',policy='legal_argmax',split=split,
                              seeds=seeds,limit=limit,reward_shaping=0.),
                candidates=records,
                differences=[dict(left_sha256=records[0]['sha256'],right_sha256=r['sha256'],
                                  metric='lines',result=paired_difference(records[0]['rows'],r['rows']))
                             for r in records[1:]])


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('checkpoints',nargs='+');parser.add_argument('--module-dir',required=True)
    parser.add_argument('--seeds',type=int,nargs='+',required=True)
    parser.add_argument('--limit',type=int,required=True)
    parser.add_argument('--split',choices=('validation','test'),required=True)
    parser.add_argument('--out',type=Path,required=True)
    args=parser.parse_args()
    if args.out.exists():raise FileExistsError(args.out)
    report=compare(args.checkpoints,seeds=args.seeds,limit=args.limit,module_dir=args.module_dir,split=args.split)
    args.out.parent.mkdir(parents=True,exist_ok=True)
    write_once(args.out,report)
    print('report:',args.out)


if __name__=='__main__':main()
