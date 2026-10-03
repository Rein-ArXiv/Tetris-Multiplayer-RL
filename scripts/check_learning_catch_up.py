"""Check time conservation without a browser or graphics context."""
from pathlib import Path
import os
import subprocess
ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / 'docs/learn/checkpoints/46-catch-up'
OUT = ROOT / 'out/learning-checkpoints/46-catch-up-check'

def run(args, **kwargs):
    result = subprocess.run(args, cwd=ROOT, text=True, capture_output=True,
                            timeout=kwargs.pop('timeout', 180), **kwargs)
    if result.returncode:
        raise RuntimeError(f'{args}\n{result.stdout}\n{result.stderr}')
    return result

def main():
    OUT.mkdir(parents=True, exist_ok=True)
    old = ROOT / 'docs/learn/checkpoints/45-clock-accounting'
    for path in old.rglob('*'):
        if path.is_file() and path.name not in ['README.md', 'CMakeLists.txt']:
            assert path.read_bytes() == (SOURCE / path.relative_to(old)).read_bytes()
    env = {**os.environ, 'SDL_VIDEODRIVER': 'unavailable', 'DISPLAY': '', 'WAYLAND_DISPLAY': ''}
    for backend in ['SCRIPTED', 'SDL']:
        build = OUT / backend.lower()
        run(['cmake', '-S', str(SOURCE), '-B', str(build), '-DSTUDY_PLATFORM=' + backend,
             '-DCMAKE_BUILD_TYPE=Release', '-DCMAKE_CXX_FLAGS=-Wall -Wextra -Wpedantic'])
        targets = [] if backend == 'SCRIPTED' else ['--target', 'budget_trace', 'budget_contract', 'frame_contract', 'clock_accounting']
        result = run(['cmake', '--build', str(build), '-j3', *targets], timeout=300)
        (OUT / f'build-{backend}.log').write_text(result.stdout + result.stderr)
        assert 'warning:' not in result.stdout + result.stderr
        pattern = [] if backend == 'SCRIPTED' else ['-R', '^(clock_accounting|frame_contract|budget_contract)$']
        result = run(['ctest', '--test-dir', str(build), '--output-on-failure', *pattern])
        (OUT / f'ctest-{backend}.log').write_text(result.stdout)
        print(result.stdout.strip(), flush=True)
        for name in ['budget_trace', 'budget_contract']:
            link = (build / 'CMakeFiles' / (name + '.dir') / 'link.txt').read_text()
            assert not any(x in link for x in ['SDL', 'study_gl', 'study_platform'])
            result = run([str(build / name)], env=env)
            (OUT / f'{name}-{backend}.log').write_text(result.stdout)
            if name == 'budget_trace':
                rows = result.stdout.splitlines()
                expected = [
                    'clamp_elapsed,burst,1,1000000000,6,0,0,900000000,0',
                    'clamp_elapsed,burst,2,0,0,0,0,0,0',
                    'keep_backlog,burst,1,1000000000,6,54,0,0,0',
                    'keep_backlog,burst,2,0,6,48,0,0,0',
                    'keep_backlog,burst,3,0,6,42,0,0,0',
                    'discard_backlog,burst,1,1000000000,6,0,0,0,54',
                    'discard_backlog,burst,2,0,0,0,0,0,0'
                ]
                for row in expected:
                    assert row in rows, row
                for policy in ['clamp_elapsed', 'keep_backlog', 'discard_backlog']:
                    assert f'{policy},boundary,1,16666667,1,0,20,0,0' in rows

    flags = ['-std=c++17', '-O1', '-DNDEBUG', '-fsanitize=undefined', '-fno-sanitize-recover=all']
    run(['c++', *flags, '-I' + str(SOURCE), str(SOURCE / 'tests/budget_contract.cpp'), '-o', str(OUT / 'sanitized')])
    print(run([str(OUT / 'sanitized')], env=env).stdout.strip(), flush=True)
    for name, old_text, new_text in [
        ('lose_backlog', 'next_credits = total - static_cast<std::uint64_t>(issued) * units;', 'next_credits = total % units;'),
        ('hide_dropped_time', 'dropped_ticks = whole - issued;', 'dropped_ticks = 0;'),
        ('unchecked_addition', 'if (accepted_ns > room / rate) return std::nullopt;', '(void)room;')
    ]:
        mutation = OUT / name
        (mutation / 'timing').mkdir(parents=True, exist_ok=True)
        text = (SOURCE / 'timing/catch_up_clock.h').read_text()
        assert old_text in text
        (mutation / 'timing/catch_up_clock.h').write_text(text.replace(old_text, new_text))
        binary = mutation / 'check'
        run(['c++', '-std=c++17', '-O1', '-DNDEBUG', '-I' + str(mutation), '-I' + str(SOURCE),
             str(SOURCE / 'tests/budget_contract.cpp'), '-o', str(binary)])
        result = subprocess.run([str(binary)], capture_output=True, text=True, timeout=10)
        assert result.returncode == 1 and 'CHECK failed' in result.stderr
        (OUT / (name + '.log')).write_text(result.stderr)
    print('Three policies: 30,000 prefix checks, 2,000 FixedClock comparisons, overflow preservation, exact trace and three Release mutations passed.', flush=True)

if __name__ == '__main__':
    main()
