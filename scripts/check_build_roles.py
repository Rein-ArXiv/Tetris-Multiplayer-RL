"""Configure real role presets and inspect CMake's generated target graphs.

Requires the SDKs for the selected roles. This does not launch game/services.
"""
from pathlib import Path
import argparse
import json
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
PRODUCTS = {'tetris', 'tetris_relay', 'tetris_relay_reactor', 'tetris_meta', 'tetris_py'}
EXPECTED = {'client': {'tetris'}, 'servers': {'tetris_relay', 'tetris_meta'},
            'training': {'tetris_py'}, 'checks': set()}

def run(command):
    result = subprocess.run(command, cwd=ROOT, capture_output=True, text=True)
    if result.returncode:
        raise RuntimeError(f'{command}\n{result.stdout}\n{result.stderr}')
    return result.stdout

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--python', default=sys.executable)
    parser.add_argument('--roles', nargs='+', choices=EXPECTED, default=list(EXPECTED))
    args = parser.parse_args()
    for role in args.roles:
        build = ROOT/'out/build'/role
        query = build/'.cmake/api/v1/query';query.mkdir(parents=True, exist_ok=True)
        (query/'codemodel-v2').touch()
        command = ['cmake', '--preset', role]
        if role == 'training':
            package = run([args.python, '-m', 'pybind11', '--cmakedir']).strip()
            command += ['-DPython_EXECUTABLE='+args.python, '-Dpybind11_DIR='+package]
        run(command)
        reply = build/'.cmake/api/v1/reply'
        index = max(reply.glob('index-*.json'), key=lambda p: p.stat().st_mtime_ns)
        entry = json.loads(index.read_text())['reply']['codemodel-v2']
        model = json.loads((reply/entry['jsonFile']).read_text())
        for config in model['configurations']:
            targets = {t['name']:t for t in config['targets']}
            assert set(targets) & PRODUCTS == EXPECTED[role], (role, targets.keys())
            if role == 'client':
                game = json.loads((reply/targets['tetris']['jsonFile']).read_text())
                dependencies = {d['id'] for d in game.get('dependencies', [])}
                assert targets['copy_assets']['id'] in dependencies
            if role == 'checks':
                assert {'sim_hash_dump', 'bot_replay_test'} <= set(targets)
                # CTest's discovery confirms registration, not just target compilation.
                tests = json.loads(run(['ctest','--test-dir',str(build),'-C','Release','--show-only=json-v1']))
                assert tests['tests'], 'The checks role must register executable tests'
        print(role, ': generated product graph matches the selected role')

if __name__ == '__main__':
    main()
