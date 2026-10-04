"""Observe private publication boundaries with synthetic, caller-owned files."""
import argparse
import json
import os
from pathlib import Path
import stat
import subprocess
import sys
import tempfile
from check_learning_text_layout import run
from check_part_docs import normalized
from check_learning_utf8 import cut

ROOT = Path(__file__).resolve().parents[1]
CP = ROOT / 'docs/learn/checkpoints/171-private-publication'
OUT = ROOT / 'out/learning-checkpoints/171-private-publication-check'
OLD = b'A' * 4096 + b'\n'
NEW = b'B' * 4096 + b'\n'


def invoke(binary, target, mode='write-new', fault=None):
    env = dict(os.environ)
    env.pop('LD_PRELOAD', None)
    env.pop('STUDY_FILE_FAULT', None)
    if fault:
        env.update(LD_PRELOAD=str(OUT/'private-file-faults.so'), STUDY_FILE_FAULT=fault)
    return subprocess.run([str(binary), str(target), mode], cwd=ROOT, env=env,
                          capture_output=True, text=True, timeout=10)


def observe(binary, faults=False):
    with tempfile.TemporaryDirectory(prefix='study171-') as tmp:
        folder = Path(tmp) / '비공개 공간 🎮'
        folder.mkdir()
        target = folder / 'synthetic.txt'
        initial = invoke(binary, target, 'write-old')
        assert (initial.returncode, initial.stdout) == (0, 'write-confirmed\n'), initial.stderr
        assert target.read_bytes() == OLD
        if os.name == 'posix':
            assert stat.S_IMODE(target.stat().st_mode) == 0o600
            # A retained open descriptor still refers to the old object after rename.
            with target.open('rb', buffering=0) as old_reader:
                assert invoke(binary, target).returncode == 0
                assert old_reader.read() == OLD
                assert target.read_bytes() == NEW
        else:
            assert invoke(binary, target).returncode == 0
            assert target.read_bytes() == NEW
        assert list(folder.iterdir()) == [target]
        target.write_bytes(OLD)
        wrong_mode = invoke(binary, target, 'bad-mode')
        assert wrong_mode.returncode == 64 and target.read_bytes() == OLD
        blocked = folder / 'not-a-file'
        blocked.mkdir()
        failed = invoke(binary, blocked)
        assert (failed.returncode, failed.stdout) == (2, 'write-not-confirmed\n')
        assert blocked.is_dir() and not list(blocked.iterdir())
        assert not list(folder.glob('*.tmp-*'))
        blocked.rmdir()
        if faults:
            for mode in ('short', 'interrupted', 'short-failure', 'file-sync', 'rename', 'directory-sync'):
                target.write_bytes(OLD)
                result = invoke(binary, target, fault=mode)
                success = mode in ('short', 'interrupted')
                assert result.returncode == (0 if success else 2), (mode, result.stderr)
                assert result.stdout == ('write-confirmed\n' if success else 'write-not-confirmed\n')
                published = success or mode == 'directory-sync'
                assert target.read_bytes() == (NEW if published else OLD), mode
                assert list(folder.iterdir()) == [target], mode
                assert stat.S_IMODE(target.stat().st_mode) == 0o600
                print(binary.name, mode, 'new complete bytes' if published else 'old bytes', flush=True)
    print(binary.name, 'publication, Unicode path, cleanup and misuse contracts passed', flush=True)


def snippets():
    previous = CP.with_name('170-windows-port')
    for path in previous.rglob('*'):
        if path.is_file() and '__pycache__' not in path.parts:
            rel = path.relative_to(previous)
            if rel.as_posix() not in ('README.md', 'roles/CMakeLists.txt'):
                assert (CP/rel).read_bytes() == path.read_bytes(), rel
    assert cut((ROOT/'meta/private_file.cpp').read_text(), 'bool write_private_file(') == cut(
        (CP/'settings/private_file.cpp').read_text(), 'bool write_private_file(')
    manuscript = ROOT/'docs/learn/lessons/171.json'
    if manuscript.exists():
        sources = [normalized(p.read_text(), 'cpp') for p in CP.rglob('*')
                   if p.suffix in ('.cpp', '.h')]
        count = 0
        for section in json.loads(manuscript.read_text())['sections']:
            ref = section.get('reference')
            if ref:
                assert ref['symbol'] in (ROOT/ref['path']).read_text(), ref
            for code in section.get('codes', []):
                if 'text' in code and code['language'] == 'cpp':
                    assert any(normalized(code['text'], 'cpp') in src for src in sources), code['label']
                    count += 1
        print('Inline snippets:', count, 'references and cumulative preservation passed', flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--snippets-only', action='store_true')
    parser.add_argument('--binary', type=Path)
    args = parser.parse_args()
    if not args.snippets_only:
        OUT.mkdir(parents=True, exist_ok=True)
        binary = args.binary
        if binary is None:
            print(run(['cmake', '-S', str(CP/'roles'), '-B', str(OUT/'build'),
                       '-DSTUDY_ROLE=STORAGE', '-DCMAKE_BUILD_TYPE=Release']).stdout, flush=True)
            print(run(['cmake', '--build', str(OUT/'build'), '--config', 'Release',
                       '--target', 'private_publish', '--parallel', '2']).stdout, flush=True)
            choices = [OUT/'build/private_publish', OUT/'build/Release/private_publish.exe',
                       OUT/'build/private_publish.exe']
            found = [p for p in choices if p.is_file()]
            assert len(found) == 1, 'pass --binary for ambiguous layouts'
            binary = found[0]
        faults = sys.platform.startswith('linux')
        if faults:
            run(['cc', '-shared', '-fPIC', str(ROOT/'tests/learning/private_file_faults.c'),
                 '-ldl', '-o', str(OUT/'private-file-faults.so')])
        observe(binary.resolve(), faults)
        if faults:
            # Compile the real product writer with only a namespace forwarding adapter.
            wrapper = OUT/'current_writer.cpp'
            wrapper.write_text('#include "meta/private_file.h"\nnamespace study_files {\n'
                'bool write_private_file(const std::string& p,const std::string& c) {\n'
                'return meta::client::write_private_file(p,c);\n}\n}\n')
            current = OUT/'current_private_publish'
            run(['c++', '-std=c++17', '-Wall', '-Wextra', '-Wpedantic', '-O1',
                 '-I'+str(ROOT), '-I'+str(CP), str(CP/'tools/private_publish.cpp'),
                 str(ROOT/'meta/private_file.cpp'), str(wrapper), '-o', str(current)])
            observe(current, True)
        print('Runtime OS:', sys.platform, '; permission/atomicity of other OS require native tests', flush=True)
    snippets()


if __name__ == '__main__':
    main()
