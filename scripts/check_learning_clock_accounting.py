"""Check time conservation without a browser or graphics context."""
from pathlib import Path
import os
import subprocess
ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / 'docs/learn/checkpoints/45-clock-accounting'
OUT = ROOT / 'out/learning-checkpoints/45-clock-accounting-check'

def run(args, **kwargs):
    result = subprocess.run(args, cwd=ROOT, text=True, capture_output=True,
                            timeout=kwargs.pop('timeout', 180), **kwargs)
    if result.returncode:
        raise RuntimeError(f'{args}\n{result.stdout}\n{result.stderr}')
    return result

def main():
    OUT.mkdir(parents=True, exist_ok=True)
    old = ROOT / 'docs/learn/checkpoints/44-frame-loop'
    for path in old.rglob('*'):
        if path.is_file() and path.name not in ['README.md', 'CMakeLists.txt']:
            assert path.read_bytes() == (SOURCE / path.relative_to(old)).read_bytes()
    env = {**os.environ, 'SDL_VIDEODRIVER': 'unavailable', 'DISPLAY': '', 'WAYLAND_DISPLAY': ''}
    for backend in ['SCRIPTED', 'SDL']:
        build = OUT / backend.lower()
        run(['cmake', '-S', str(SOURCE), '-B', str(build), '-DSTUDY_PLATFORM=' + backend,
             '-DCMAKE_BUILD_TYPE=Release', '-DCMAKE_CXX_FLAGS=-Wall -Wextra -Wpedantic'])
        targets = [] if backend == 'SCRIPTED' else ['--target', 'clock_trace', 'clock_accounting', 'frame_contract']
        result = run(['cmake', '--build', str(build), '-j3', *targets], timeout=300)
        (OUT / f'build-{backend}.log').write_text(result.stdout + result.stderr)
        assert 'warning:' not in result.stdout + result.stderr
        pattern = [] if backend == 'SCRIPTED' else ['-R', '^(clock_accounting|frame_contract)$']
        result = run(['ctest', '--test-dir', str(build), '--output-on-failure', *pattern])
        (OUT / f'ctest-{backend}.log').write_text(result.stdout)
        print(result.stdout.strip(), flush=True)
        for name in ['clock_trace', 'clock_accounting']:
            link = (build / 'CMakeFiles' / (name + '.dir') / 'link.txt').read_text()
            assert not any(x in link for x in ['SDL', 'study_gl', 'study_platform'])
            result = run([str(build / name)], env=env)
            (OUT / f'{name}-{backend}.log').write_text(result.stdout)
            if name == 'clock_trace':
                rows = result.stdout.splitlines()
                for row in ['1,8000000,0,0,480000000', '2,9000000,480000000,1,20000000',
                            '3,5000000,20000000,0,320000000', '4,11000000,320000000,0,980000000',
                            '5,17000000,980000000,2,0', '2,1,999999960,1,20']:
                    assert row in rows, row
                assert rows.count('# total_ticks=60, final_phase=0') == 2
                assert rows[-1] == '1,1,0,0,60'
    flags = ['-std=c++17', '-O1', '-DNDEBUG', '-fsanitize=undefined', '-fno-sanitize-recover=all']
    run(['c++', *flags, '-I' + str(SOURCE), str(SOURCE / 'tests/clock_accounting.cpp'), '-o', str(OUT / 'sanitized')])
    print(run([str(OUT / 'sanitized')], env=env).stdout.strip(), flush=True)
    for name, old_text, new_text in [
        ('discard_remainder', 'phase_ = total % units;', 'phase_ = 0;'),
        ('rounded_tick', "units = 1'000'000'000", "units = 999'999'960")
    ]:
        mutation = OUT / name
        (mutation / 'timing').mkdir(parents=True, exist_ok=True)
        text = (SOURCE / 'timing/fixed_clock.h').read_text()
        assert old_text in text
        (mutation / 'timing/fixed_clock.h').write_text(text.replace(old_text, new_text))
        binary = mutation / 'check'
        run(['c++', '-std=c++17', '-O1', '-DNDEBUG', '-I' + str(mutation), '-I' + str(SOURCE),
             str(SOURCE / 'tests/clock_accounting.cpp'), '-o', str(binary)])
        result = subprocess.run([str(binary)], capture_output=True, text=True, timeout=10)
        assert result.returncode == 1 and 'CHECK failed:' in result.stderr
        (OUT / (name + '.log')).write_text(result.stderr)
    print('CPU trace, 10,000 prefix conservation checks, independent partitions, quantization, rollback and two Release mutations passed.', flush=True)

if __name__ == '__main__':
    main()
