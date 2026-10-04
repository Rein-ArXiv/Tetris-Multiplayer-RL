"""A small CPU experiment supervisor: durable files outlive child memory."""
import argparse
from datetime import datetime, timezone
import hashlib
import importlib.machinery
import importlib.metadata
import json
from pathlib import Path
import re
import sys
from process_runner import run_logged

# Example work budgets. These describe execution amount, not bot difficulty.
PRESETS = {'smoke': 9, 'long': 10000}
CP = Path(__file__).resolve().parents[1]


def write_once(path, data):
    with Path(path).open('x', encoding='utf-8') as stream:
        json.dump(data, stream, ensure_ascii=False, indent=2, allow_nan=False)
        stream.write('\n')


def hash_file(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def source_hash():
    state = hashlib.sha256()
    for path in sorted(p for p in CP.rglob('*') if p.is_file() and p.suffix in ('.py','.h','.cpp')
                       and '__pycache__' not in p.parts):
        state.update(path.relative_to(CP).as_posix().encode()+b'\0'+path.read_bytes()+b'\0')
    return state.hexdigest()


def identity(module_dir):
    directory = Path(module_dir).resolve()
    matches = [directory/('study_py'+suffix) for suffix in importlib.machinery.EXTENSION_SUFFIXES
               if (directory/('study_py'+suffix)).is_file()]
    if len(matches) != 1:
        raise ValueError('select one freshly built extension for this Python')
    return dict(source=source_hash(), module=str(matches[0]), module_hash=hash_file(matches[0]),
                python=sys.executable, python_version=sys.version.split()[0],
                packages={name:importlib.metadata.version(name) for name in ('torch','numpy','gymnasium')})


def launch(*, module_dir, output_root, name, preset, smoke_run=None):
    if preset not in PRESETS or not re.fullmatch(r'[a-z0-9_-]{1,64}', name):
        raise ValueError('unknown preset or invalid run name')
    contract = identity(module_dir)
    if preset == 'long':
        if smoke_run is None:
            raise ValueError('long needs a matching successful smoke directory')
        before = json.loads((Path(smoke_run)/'manifest.json').read_text())
        done = json.loads((Path(smoke_run)/'result.json').read_text())
        if before['preset'] != 'smoke' or before['contract'] != contract or done['status'] != 'success':
            raise ValueError('smoke contract mismatch')
    directory = Path(output_root).resolve()/name
    directory.mkdir(parents=True, exist_ok=False)
    command = [sys.executable, '-u', str(Path(__file__).resolve()), 'worker',
               '--module-dir', str(Path(module_dir).resolve()), '--directory', str(directory)]
    manifest = dict(started_at=datetime.now(timezone.utc).isoformat(), preset=preset,
                    budget=PRESETS[preset], contract=contract, command=command)
    write_once(directory/'manifest.json', manifest)
    try:
        run_logged(command, cwd=CP, log_path=directory/'train.log')
        checkpoint = directory/'training.pt'
        if not checkpoint.is_file():
            raise RuntimeError('worker did not save the training state')
        write_once(directory/'result.json', dict(status='success', checkpoint_hash=hash_file(checkpoint)))
    except BaseException as error:
        write_once(directory/'result.json', dict(status='failed', error_type=type(error).__name__))
        raise
    return directory


def worker(directory, module_dir):
    directory = Path(directory)
    manifest = json.loads((directory/'manifest.json').read_text())
    if identity(module_dir) != manifest['contract']:
        raise RuntimeError('source or native artifact changed before worker start')
    sys.path.insert(0, str(Path(module_dir).resolve()))
    import study_py
    if str(Path(study_py.__file__).resolve()) != manifest['contract']['module']:
        raise RuntimeError('unexpected module loaded')
    import torch
    from training_checkpoint import TrainingRun
    torch.set_num_threads(1)
    run = TrainingRun()
    try:
        budget = manifest['budget']
        while run.decisions < budget:
            data, metrics = run.advance(min(run.config.rollout, budget-run.decisions))
            run.save(directory/'training.pt')
            print(json.dumps(dict(decisions=run.decisions, updates=run.updates,
                                  episodes=len(run.collector.completed), metrics=metrics),allow_nan=False),flush=True)
        print('worker completed',run.decisions,flush=True)
    finally:
        run.close()


def main():
    parser=argparse.ArgumentParser()
    sub=parser.add_subparsers(dest='task',required=True)
    p=sub.add_parser('launch');p.add_argument('--module-dir',required=True)
    p.add_argument('--output-root',required=True);p.add_argument('--name',required=True)
    p.add_argument('--preset',choices=tuple(PRESETS),default='smoke');p.add_argument('--smoke-run')
    p=sub.add_parser('worker');p.add_argument('--module-dir',required=True);p.add_argument('--directory',required=True)
    args=vars(parser.parse_args());task=args.pop('task')
    if task=='worker':worker(**args)
    else:print('completed:',launch(**args))


if __name__=='__main__':main()
