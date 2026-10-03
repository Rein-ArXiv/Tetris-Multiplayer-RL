"""Check CPU vertex data, without claiming a GPU upload or window display."""
from pathlib import Path
import json
import subprocess

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / 'docs/learn/checkpoints/13-vertex-data'
OUT = ROOT / 'out/learning-checkpoints/13-vertex-data'


def run(args):
    result = subprocess.run(args, cwd=ROOT, text=True, capture_output=True, timeout=60)
    if result.returncode:
        raise RuntimeError(f'{args}\n{result.stdout}\n{result.stderr}')
    return result


def main():
    expected = ['count=3', 'stride=8', 'bytes=24', 'offset x=0', 'offset y=4',
                'v0=(-0.5, -0.5)', 'v1=(0.5, -0.5)', 'v2=(0, 0.5)',
                'after reserve size=0 cap>=3 1', 'after push size=3 cap>=3 1',
                'copy matches=1', 'after clear size=0', 'capacity unchanged 1']
    for backend in ['SCRIPTED', 'SDL']:
        build = OUT / backend.lower()
        run(['cmake', '-S', str(SOURCE), '-B', str(build),
             '-DSTUDY_PLATFORM=' + backend, '-DCMAKE_BUILD_TYPE=Release',
             '-DCMAKE_EXPORT_COMPILE_COMMANDS=ON',
             '-DCMAKE_CXX_FLAGS=-Wall -Wextra -Wpedantic'])
        result = run(['cmake', '--build', str(build), '-j2'])
        if 'warning:' in result.stderr:
            raise RuntimeError(result.stderr)
        run(['ctest', '--test-dir', str(build), '--output-on-failure'])
        assert run([str(build / 'layout_demo')]).stdout.splitlines() == expected
        commands = json.loads((build / 'compile_commands.json').read_text())
        cpu = next(c['command'] for c in commands if c['file'].endswith('/src/layout_demo.cpp'))
        assert 'SDL2' not in cpu
        if backend == 'SCRIPTED':
            assert not (build / 'tetris').exists()
            assert not any('/platform/sdl.cpp' in c['file'] for c in commands)
        else:
            assert (build / 'tetris').exists()
        print(f'{backend}: Release build, CPU output and loader/mesh CTest passed')

    # A changed field must fail the promised upload layout at compile time.
    bad = OUT / 'bad-layout' / 'renderer'
    bad.mkdir(parents=True, exist_ok=True)
    (bad / 'mesh.h').write_text((SOURCE / 'renderer/mesh.h').read_text().replace('float y;', 'double y;'))
    result = subprocess.run(['c++', '-std=c++17', '-I' + str(bad.parent),
                             '-c', str(SOURCE / 'src/layout_demo.cpp'),
                             '-o', str(OUT / 'bad-layout.o')], cwd=ROOT, text=True,
                            capture_output=True, timeout=60)
    assert result.returncode != 0 and 'static assertion failed' in result.stderr

    # The data test must catch a wrong coordinate, not only re-evaluate sizeof.
    wrong = OUT / 'wrong-data' / 'renderer'
    wrong.mkdir(parents=True, exist_ok=True)
    (wrong / 'mesh.h').write_text((SOURCE / 'renderer/mesh.h').read_text().replace('Vertex2{ 0.0f,  0.5f}', 'Vertex2{ 0.0f, -0.5f}'))
    binary = OUT / 'wrong-data-test'
    run(['c++', '-std=c++17', '-DNDEBUG', '-I' + str(wrong.parent),
         str(SOURCE / 'tests/mesh_contract.cpp'), '-o', str(binary)])
    result = subprocess.run([str(binary)], capture_output=True, text=True, timeout=10)
    assert result.returncode == 1 and 'FAIL: v2' in result.stdout
    print('Negative controls: wrong layout rejected; wrong vertex detected under NDEBUG')
    print('CPU data only. Native GUI, GPU upload and other OSes not tested.')


if __name__ == '__main__':
    main()
