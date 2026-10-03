"""Inject metadata read failures into the actual DB implementation using disposable DBs."""
from pathlib import Path
import tempfile
from check_learning_text_layout import run

ROOT = Path(__file__).resolve().parents[1]
CP = ROOT / 'docs/learn/checkpoints/112-migrations'
OUT = ROOT / 'out/learning-checkpoints/112-migrations-check'


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    build = OUT / 'scripted'
    run(['cmake', '-S', str(CP), '-B', str(build), '-DSTUDY_PLATFORM=SCRIPTED',
         '-DCMAKE_BUILD_TYPE=Release'])
    run(['cmake', '--build', str(build), '--target', 'study_sqlite', '-j2'])
    binary = OUT / 'current-migrations'
    run(['c++', '-std=c++17', '-pthread', '-Wall', '-Wextra', '-Wpedantic',
         '-fsanitize=address,undefined', '-fno-sanitize-recover=all',
         '-I' + str(ROOT), '-I' + str(CP), '-isystem', str(ROOT / 'third_party'),
         str(ROOT / 'tests/learning/current_migrations.cpp'),
         str(ROOT / 'meta/credentials.cpp'), str(build / 'libstudy_sqlite.a'),
         '-lcrypto', '-ldl', '-o', str(binary)])
    with tempfile.TemporaryDirectory(prefix='root-migrations-') as tmp:
        print(run([str(binary), str(Path(tmp) / 'fixture-')]).stdout, flush=True)


if __name__ == '__main__':
    main()
