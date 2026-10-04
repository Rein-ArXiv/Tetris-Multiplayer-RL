"""Compare canonical policy artifacts on one explicit single-board protocol.

Run through train.colab_runtime launch to select the prepared native module.
Keep validation seeds separate from the held-out final test set. Results here
measure placement play, not timed versus wins or a character difficulty tier.
"""
from datetime import datetime, timezone
import hashlib
import importlib.metadata
import json
from pathlib import Path
import sys


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def compare(paths, *, seeds, max_pieces, split, device='cpu'):
    from common.checkpoint import load_checkpoint, IO_CONTRACT
    from train.rl_common import evaluate_episodes
    from train.evaluation_summary import summarize, paired_difference
    from train.colab_runtime import source_digest
    import torch
    import tetris_py
    if split not in ('validation','test'):
        raise ValueError('split must be validation or test')
    paths = [Path(path).resolve() for path in paths]
    seeds = list(seeds)
    if not paths or len(set(paths)) != len(paths):
        raise ValueError('provide distinct checkpoint paths')
    native = Path(tetris_py.__file__).resolve()
    root = Path(__file__).resolve().parents[2]
    before = source_digest(root)
    native_hash = digest(native)
    records = []
    # Loading a candidate initializes modules before loading weights. Preserve
    # the caller's CPU RNG even though evaluation itself uses greedy actions.
    with torch.random.fork_rng(devices=[]):
        for path in paths:
            artifact_hash = digest(path)
            model = load_checkpoint(path, device=device)
            if digest(path) != artifact_hash:
                raise RuntimeError('checkpoint changed while loading')
            rows = evaluate_episodes(model, seeds=seeds, device=device, max_pieces=max_pieces)
            records.append(dict(path=str(path), sha256=artifact_hash, rows=rows, summary=summarize(rows)))
    if source_digest(root) != before or digest(native) != native_hash:
        raise RuntimeError('evaluation implementation changed during comparison')
    return dict(format_version=1, created_at=datetime.now(timezone.utc).isoformat(),
                protocol=dict(environment='TetrisPlacementEnv', policy='legal_argmax',
                              split=split, seeds=seeds, max_pieces=max_pieces, io_contract=IO_CONTRACT),
                runtime=dict(python=sys.version.split()[0], device=str(device),
                             packages={name:importlib.metadata.version(name) for name in ('torch','numpy','gymnasium')},
                             source_hash=before, native_path=str(native), native_hash=native_hash),
                candidates=records,
                differences=[dict(left_sha256=records[0]['sha256'], right_sha256=row['sha256'],
                                  metric='lines', result=paired_difference(records[0]['rows'],row['rows']))
                             for row in records[1:]])


def main():
    import argparse
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('checkpoints',nargs='+')
    parser.add_argument('--seeds',nargs='+',type=int,required=True)
    parser.add_argument('--max-pieces',type=int,required=True)
    parser.add_argument('--split',choices=('validation','test'),required=True)
    parser.add_argument('--device',default='cpu')
    parser.add_argument('--out',type=Path,required=True)
    args=parser.parse_args()
    if args.out.exists():
        raise FileExistsError(args.out)
    report=compare(args.checkpoints,seeds=args.seeds,max_pieces=args.max_pieces,split=args.split,device=args.device)
    args.out.parent.mkdir(parents=True,exist_ok=True)
    # Exclusive creation keeps a second run from overwriting earlier evidence.
    with args.out.open('x',encoding='utf-8') as stream:
        json.dump(report,stream,ensure_ascii=False,indent=2,allow_nan=False)
        stream.write('\n')
    print('evaluation report:',args.out)


if __name__=='__main__':main()
