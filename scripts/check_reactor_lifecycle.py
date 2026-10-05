"""Linux reactor lifecycle regression using actual pthread failure injection.

Usage: python scripts/check_reactor_lifecycle.py out/ci-http-fix/tetris_relay_reactor
Pass --before to require the original partial-startup SIGABRT instead.
No clients or meta service; loopback listener only, no environment credentials.
"""
from pathlib import Path
import argparse
import re
import signal
import socket
import subprocess
import time

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('binary', type=Path)
    parser.add_argument('--before', action='store_true')
    args = parser.parse_args()
    out = ROOT / 'out/ci-http-fix/reactor-lifecycle'
    out.mkdir(parents=True, exist_ok=True)
    library = out / 'inject.so'
    subprocess.run(['c++', '-std=c++17', '-shared', '-fPIC', '-pthread', '-Wall',
                    '-Wextra', str(ROOT / 'tests/reactor_lifecycle_inject.cpp'),
                    '-ldl', '-o', str(library)], check=True)
    # This three-loop fixture starts front Offload(4), two shard Offload(2),
    # then two shard run threads. Fail the second shard after the first starts.
    offload_workers = 4 + 2 * 2
    fail_create = offload_workers + 2
    modes = ['startup'] if args.before else ['startup', 'front', 'shard', 'normal']
    for mode in modes:
        with socket.socket() as reservation:
            reservation.bind(('127.0.0.1', 0))
            port = reservation.getsockname()[1]
        log = out / (('before-' if args.before else '') + mode + '.log')
        env = {'LD_PRELOAD': str(library)}
        if mode == 'startup': env['RELAY_TEST_FAIL_CREATE'] = str(fail_create)
        elif mode != 'normal': env['RELAY_TEST_THROW_POLL'] = mode
        with log.open('w') as stream:
            process = subprocess.Popen([str(args.binary.resolve()), '--loops', '3',
                                        '--port', str(port), '--loopback-only'],
                                       stdout=stream, stderr=stream, env=env)
            try:
                if mode == 'normal':
                    deadline = time.monotonic() + 5
                    while 'forwarding shards: 2' not in log.read_text():
                        if process.poll() is not None or time.monotonic() > deadline:
                            raise AssertionError('three-loop startup did not complete')
                        time.sleep(0.02)
                    time.sleep(0.1)
                    process.send_signal(signal.SIGTERM)
                code = process.wait(timeout=10)
            finally:
                if process.poll() is None:
                    process.kill()
                    process.wait()
        text = log.read_text()
        if args.before:
            assert code == -signal.SIGABRT, (code, text)
            print('before: partial shard startup reproduced SIGABRT')
            continue
        assert code == (0 if mode == 'normal' else 1), (mode, code, text)
        record = re.search(r'lifecycle creates=(\d+) started=(\d+) completed=(\d+) joined=(\d+)', text)
        assert record, (mode, text)
        creates, started, completed, joined = map(int, record.groups())
        expected = offload_workers + (1 if mode == 'startup' else 2)
        assert creates == fail_create and started == completed == joined == expected, (mode, record.group())
        if mode == 'normal': assert text.count('[relay] done') == 3, text
        print(f'{mode}: exit={code}; {completed} started threads completed and joined')


if __name__ == '__main__':
    main()
