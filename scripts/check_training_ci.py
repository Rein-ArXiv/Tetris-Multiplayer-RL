#!/usr/bin/env python3
'''Standalone CLI runner for the tetris-py learning checks.

Runs an explicit list of pytest test modules against dumps produced in a
build directory. Missing dumps or third-party packages are hard failures.
'''

from __future__ import annotations

import argparse
import importlib
import os
import sys
from pathlib import Path

TESTS = [
    'action_codec_parity',
    'binding_boundary',
    'checkpoint_integrity',
    'checkpoint_roundtrip',
    'colab_workflow',
    'determinism_crossplatform',
    'determinism_reference',
    'evaluation_summary',
    'expansion_contract',
    'gym_contract',
    'heuristic_contract',
    'masked_distribution',
    'model_zoo_contract',
    'observation_parity',
    'onnx_export',
    'opponent_bundle',
    'opponent_profile',
    'placement_parity',
    'policy_contract',
    'ppo_contract',
    'returns_contract',
    'reward_contract',
    'training_scripts_static',
    'versus_boundary',
    'versus_env',
]

DUMP_ENV_VARS = {
    'action_codec_dump': 'TETRIS_ACTION_CODEC_DUMP',
    'observation_dump': 'TETRIS_OBSERVATION_DUMP',
    'heuristic_dump': 'TETRIS_HEURISTIC_DUMP',
    'opponent_profile_dump': 'TETRIS_OPPONENT_PROFILE_DUMP',
}

REQUIRED_PACKAGES = [
    'torch',
    'gymnasium',
    'numpy',
    'onnx',
    'onnxruntime',
    'onnxscript',
    'pytest',
]


def parse_args(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--module-dir', required=True, type=Path)
    parser.add_argument('--build-dir', required=True, type=Path)
    return parser.parse_args(argv)


def check_dumps(build_dir):
    for filename, env_name in DUMP_ENV_VARS.items():
        suffix = '.exe' if sys.platform == 'win32' else ''
        dump_path = build_dir / (filename + suffix)
        if not dump_path.is_file():
            raise FileNotFoundError('missing build dump: ' + str(dump_path))
        os.environ[env_name] = str(dump_path)


def import_required_packages():
    for name in REQUIRED_PACKAGES:
        importlib.import_module(name)


class SkipTracker:
    '''Pytest plugin recording every skipped collection or test node.'''

    def __init__(self):
        self.skipped = []

    def pytest_collectreport(self, report):
        if report.skipped:
            self.skipped.append(report.nodeid)

    def pytest_runtest_logreport(self, report):
        if report.skipped:
            self.skipped.append(report.nodeid)


def run_pytest(test_files):
    tracker = SkipTracker()
    pytest = importlib.import_module('pytest')
    arguments = [str(path) for path in test_files]
    arguments += ['-q', '--no-header']
    exit_code = pytest.main(arguments, plugins=[tracker])
    if exit_code == 0 and tracker.skipped:
        print('skipped tests detected:')
        for nodeid in tracker.skipped:
            print('  ' + nodeid)
        return 1
    return exit_code


def main(argv=None):
    args = parse_args(argv)
    module_dir = args.module_dir.resolve()
    build_dir = args.build_dir.resolve()

    repo_root = Path(__file__).resolve().parents[1]
    python_dir = repo_root / 'python'

    for entry in (python_dir, module_dir):
        if str(entry) not in sys.path:
            sys.path.insert(0, str(entry))

    os.environ['TETRIS_PY_MODULE_DIR'] = str(module_dir)
    check_dumps(build_dir)
    import_required_packages()

    tetris_py = importlib.import_module('tetris_py')
    tetris_py_parent = Path(tetris_py.__file__).resolve().parent
    if tetris_py_parent != module_dir:
        raise RuntimeError(
            'tetris_py imported from ' + str(tetris_py_parent)
            + ', expected ' + str(module_dir)
        )

    print('Native extension:', tetris_py.__file__, flush=True)
    test_files = [python_dir / 'tests' / ('test_' + stem + '.py') for stem in TESTS]
    return run_pytest(test_files)


if __name__ == '__main__':
    raise SystemExit(main())
