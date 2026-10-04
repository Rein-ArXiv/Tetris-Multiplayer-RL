"""Reproducible headless preparation and fresh-process experiment launch.

No clone/pull/install happens on import. Notebook cells explicitly choose their
repository and dependencies. Preparation creates a new private build directory.
"""
from datetime import datetime, timezone
import argparse
import hashlib
import importlib.machinery
import importlib.metadata
import json
import os
from pathlib import Path
import platform
import re
import runpy
import subprocess
import sys
import tempfile
from .process_runner import run_logged
from .training_commands import command_for


def checked(argv, *, cwd):
    result = subprocess.run(argv, cwd=cwd, text=True, capture_output=True)
    if result.returncode:
        print(result.stdout, end='')
        print(result.stderr, end='', file=sys.stderr)
        result.check_returncode()
    return result


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def source_digest(repo):
    """Hash selected build/training sources, never notebooks' outputs or user data."""
    repo = Path(repo).resolve()
    paths = [repo/'CMakeLists.txt', repo/'python/requirements.txt', repo/'python/requirements-colab.txt']
    for folder in ('bindings', 'src', 'core', 'python/common', 'python/train', 'python/sim', 'python/netbot'):
        paths.extend(p for p in (repo/folder).rglob('*')
                     if p.is_file() and p.suffix in ('.cpp', '.h', '.hpp', '.py') and '__pycache__' not in p.parts)
    h = hashlib.sha256()
    for path in sorted(set(paths)):
        h.update(path.relative_to(repo).as_posix().encode()+b'\0'+path.read_bytes()+b'\0')
    return h.hexdigest()


def write_once(path, data):
    with Path(path).open('x', encoding='utf-8') as stream:
        json.dump(data, stream, ensure_ascii=False, indent=2, allow_nan=False)
        stream.write('\n')


def package_versions():
    versions = {}
    for name in ('pybind11', 'numpy', 'torch', 'gymnasium', 'onnx', 'onnxscript', 'onnxruntime'):
        try:
            versions[name] = importlib.metadata.version(name)
        except importlib.metadata.PackageNotFoundError:
            versions[name] = None
    return versions


def prepare_native(repo, *, jobs=2):
    repo = Path(repo).resolve()
    if type(jobs) is not int or jobs < 1:
        raise ValueError('jobs must be positive')
    verify_helpers()
    before = source_digest(repo)
    parent = repo/'out/colab-native'
    parent.mkdir(parents=True, exist_ok=True)
    build = Path(tempfile.mkdtemp(prefix='build-', dir=parent))
    pybind = checked([sys.executable, '-m', 'pybind11', '--cmakedir'], cwd=repo).stdout.strip()
    run_logged(['cmake', '-S', str(repo), '-B', str(build), '-DCMAKE_BUILD_TYPE=Release',
                '-DTETRIS_BUILD_GAME=OFF', '-DTETRIS_BUILD_PY=ON', '-DTETRIS_BUILD_TEST=ON',
                '-DPython_EXECUTABLE='+sys.executable, '-Dpybind11_DIR='+pybind],
               cwd=repo, log_path=build/'configure.log')
    run_logged(['cmake', '--build', str(build), '--config', 'Release', '--parallel', str(jobs),
                '--target', 'tetris_py', 'sim_hash_dump'], cwd=repo, log_path=build/'build.log')
    candidates = [p for p in build.rglob('tetris_py*') if p.is_file()
                  and any(p.name == 'tetris_py'+suffix for suffix in importlib.machinery.EXTENSION_SUFFIXES)]
    if len(candidates) != 1:
        raise RuntimeError('expected exactly one extension for the active Python ABI')
    if source_digest(repo) != before:
        raise RuntimeError('source changed during native build; prepare again')
    artifact = candidates[0]
    receipt = dict(repo=str(repo), module_dir=str(artifact.parent), module=str(artifact),
                   module_sha256=digest(artifact), source_sha256=before,
                   python=sys.executable, python_version=platform.python_version(), packages=package_versions(),
                   dirty=bool(checked(['git', 'status', '--porcelain'], cwd=repo).stdout.strip()),
                   commit=checked(['git', 'rev-parse', 'HEAD'], cwd=repo).stdout.strip())
    write_once(build/'preparation.json', receipt)
    return receipt


def verify_preparation(receipt):
    verify_helpers()
    repo = Path(receipt['repo'])
    if (receipt['python'] != sys.executable or receipt['python_version'] != platform.python_version()
            or receipt['packages'] != package_versions()
            or receipt['module_sha256'] != digest(receipt['module'])
            or receipt['source_sha256'] != source_digest(repo)):
        raise RuntimeError('preparation is stale; rebuild and repeat smoke')
    if Path(receipt['module']).parent != Path(receipt['module_dir']):
        raise ValueError('module location mismatch')


def import_native(directory):
    directory = Path(directory).resolve()
    sys.path.insert(0, str(directory))
    import tetris_py
    if Path(tetris_py.__file__).resolve().parent != directory:
        raise RuntimeError('another native module is already loaded; use a new process')
    return tetris_py


def launch_argv(receipt, command):
    if len(command) < 3 or command[:2] != [sys.executable, '-m']:
        raise ValueError('expected an active-Python module command')
    return [sys.executable, '-u', '-m', 'train.colab_runtime', 'launch',
            '--module-dir', receipt['module_dir'], '--module', command[2], '--', *command[3:]]


def run_smoke(receipt):
    verify_preparation(receipt)
    result = checked([sys.executable, '-m', 'train.colab_runtime', 'probe',
                      '--module-dir', receipt['module_dir']], cwd=Path(receipt['repo'])/'python')
    if result.stderr:
        print(result.stderr, end='')
    report = json.loads(result.stdout.splitlines()[-1])
    if report['module_sha256'] != receipt['module_sha256']:
        raise RuntimeError('smoke imported a different module')
    ready = {**receipt, 'smoke': report}
    print(json.dumps(report, ensure_ascii=False, indent=2))
    return ready


def run_experiment(ready, *, algo, run_name, preset, checkpoint_dir, smoke_run=None):
    verify_preparation(ready)
    if 'smoke' not in ready or ready['smoke']['module_sha256'] != ready['module_sha256']:
        raise RuntimeError('run the fresh-process native smoke first')
    directory = Path(checkpoint_dir).resolve()/run_name
    command = command_for(algo, run_name, preset, directory)  # Validate before creating output.
    if preset == 'long':
        if smoke_run is None:
            raise ValueError('long preset requires a successful smoke run directory')
        prior = json.loads((Path(smoke_run)/'manifest.json').read_text())
        finish = json.loads((Path(smoke_run)/'result.json').read_text())
        if (prior['preset'] != 'smoke' or prior['algo'] != algo or finish['status'] != 'success'
                or prior['preparation'] != ready):
            raise RuntimeError('smoke does not match this algorithm/preparation')
    directory.mkdir(parents=True, exist_ok=False)
    manifest = dict(started_at=datetime.now(timezone.utc).isoformat(), algo=algo, preset=preset,
                    command=command, preparation=ready)
    write_once(directory/'manifest.json', manifest)
    try:
        run_logged(launch_argv(ready, command), cwd=Path(ready['repo'])/'python',
                   log_path=directory/'train.log')
        expected = directory/(run_name+'.pt')
        if not expected.is_file() or expected.stat().st_size == 0:
            raise RuntimeError('trainer exited without the expected checkpoint')
        artifacts = {p.name:digest(p) for p in sorted(directory.glob('*.pt'))}
        write_once(directory/'result.json', dict(status='success', artifacts=artifacts))
    except BaseException as error:
        write_once(directory/'result.json', dict(status='failed', error_type=type(error).__name__))
        raise
    return directory


def probe(directory):
    native = import_native(directory)
    import numpy as np
    import torch
    torch.set_num_threads(1)  # Small CPU probe; the actual trainer chooses its device.
    from common.env import TetrisPlacementEnv
    from common.models import TetrisPolicyNet, masked_log_softmax
    from common.checkpoint import save_checkpoint, load_checkpoint
    game = native.SimGame(42)
    branch = game.clone()
    if branch.state_hash() != game.state_hash():
        raise RuntimeError('clone differs before mutation')
    placement = branch.legal_placements()[0]
    cleared = branch.apply_placement(placement.col, placement.rot)
    if cleared < 0:  # Zero cleared lines is still a successful placement.
        raise RuntimeError('legal placement was rejected')
    if branch.state_hash() == game.state_hash():
        raise RuntimeError('clone mutation did not remain isolated')
    env = TetrisPlacementEnv(seed=42)
    try:
        observation, info = env.reset(seed=42)
        action = int(np.flatnonzero(info['legal_mask'])[0])
        env.step(action)
        model = TetrisPolicyNet()
        inputs = [torch.as_tensor(observation[k]).unsqueeze(0) for k in ('board', 'current', 'next')]
        logits, value = model(*inputs)
        mask = torch.as_tensor(info['legal_mask']).unsqueeze(0)
        loss = -masked_log_softmax(logits, mask)[0,action] + value.square().mean()
        optimizer = torch.optim.Adam(model.parameters())
        loss.backward()
        torch.nn.utils.clip_grad_norm_(model.parameters(), 1., error_if_nonfinite=True)
        optimizer.step()
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder)/'model.pt'; save_checkpoint(model, path)
            loaded = load_checkpoint(path)
            if not all(torch.equal(v, loaded.state_dict()[k]) for k,v in model.state_dict().items()):
                raise RuntimeError('checkpoint weights differ after reload')
        print(json.dumps(dict(module=str(Path(native.__file__).resolve()), module_sha256=digest(native.__file__),
                              python=platform.python_version(), torch=str(torch.__version__),
                              numpy=np.__version__, gymnasium=importlib.metadata.version('gymnasium'),
                              cuda_available=torch.cuda.is_available(), observation_shapes={k:list(v.shape) for k,v in observation.items()},
                              legal_actions=int(info['legal_mask'].sum()), action_space=int(info['legal_mask'].size),
                              native_step=True, model_update=True, checkpoint_roundtrip=True)))
    finally:
        env.close()


def main():
    parser = argparse.ArgumentParser()
    sub = parser.add_subparsers(dest='task', required=True)
    p = sub.add_parser('probe');p.add_argument('--module-dir', required=True)
    p = sub.add_parser('launch');p.add_argument('--module-dir', required=True);p.add_argument('--module', required=True)
    p.add_argument('args', nargs=argparse.REMAINDER)
    args = parser.parse_args()
    if args.task == 'probe':
        probe(args.module_dir)
    else:
        import_native(args.module_dir)
        remaining = args.args[1:] if args.args[:1] == ['--'] else args.args
        sys.argv = [args.module, *remaining]
        runpy.run_module(args.module, run_name='__main__')


_HELPER_FILES = [Path(__file__), Path(__file__).with_name('process_runner.py'),
                 Path(__file__).with_name('training_commands.py')]
_LOADED_HELPERS = {str(path):digest(path) for path in _HELPER_FILES}


def verify_helpers():
    if any(digest(path) != expected for path,expected in _LOADED_HELPERS.items()):
        raise RuntimeError('launcher source changed in this kernel; restart before preparation')


if __name__ == '__main__': main()
