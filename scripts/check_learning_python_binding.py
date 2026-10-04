"""Lesson149: real extension calls, direct C++ parity and boundary ownership."""
from pathlib import Path
import argparse
import json
import sys
from check_learning_text_layout import run
from check_part_docs import normalized

R = Path(__file__).resolve().parents[1]
CP = R / 'docs/learn/checkpoints/149-python-binding'
OUT = R / 'out/learning-checkpoints/149-python-binding-check'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--python', default=sys.executable)
    parser.add_argument('--snippets-only', action='store_true')
    args = parser.parse_args()
    previous = CP.with_name('148-abuse-review')
    for path in previous.rglob('*'):
        if path.is_file() and path.relative_to(previous).as_posix() not in {'README.md','CMakeLists.txt'}:
            assert path.read_bytes() == (CP/path.relative_to(previous)).read_bytes(), path
    OUT.mkdir(parents=True, exist_ok=True)
    if not args.snippets_only:
        pybind = run([args.python,'-m','pybind11','--cmakedir']).stdout.strip()
        build = OUT/'native'
        run(['cmake','-S',str(CP/'bindings'),'-B',str(build),'-DCMAKE_BUILD_TYPE=Release',
             '-DPython_EXECUTABLE='+args.python,'-Dpybind11_DIR='+pybind,
             '-DCMAKE_CXX_FLAGS=-Wall -Wextra -Wpedantic'])
        result = run(['cmake','--build',str(build),'--config','Release','-j2'],timeout=180)
        (OUT/'build.log').write_text(result.stdout+result.stderr)
        print(run(['ctest','--test-dir',str(build),'-C','Release','--output-on-failure'],timeout=60).stdout)
        result = run([args.python,str(CP/'bindings/demo.py'),'--module-dir',str(build/'python/Release')])
        assert result.stdout.count('True') == 4 and 'False' not in result.stdout
        print(result.stdout)
        sanitized = OUT/'session-sanitized'
        run(['c++','-std=c++17','-Wall','-Wextra','-Wpedantic','-O1','-g',
             '-fsanitize=address,undefined','-fno-sanitize-recover=all','-I'+str(CP),
             str(CP/'tests/session_contract.cpp'),'-o',str(sanitized)],timeout=180)
        run([str(sanitized)],timeout=30)
        print('Session native adapter ASan/UBSan passed (Python runtime not instrumented)')
    manuscript = R/'docs/learn/lessons/149.json'
    if manuscript.exists():
        corpus = [normalized(p.read_text(),'cpp') for p in CP.rglob('*') if p.suffix in {'.h','.cpp'}]
        count = 0
        for section in json.loads(manuscript.read_text())['sections']:
            ref = section.get('reference')
            if ref:
                assert ref['symbol'] in (R/ref['path']).read_text(), ref
            for code in section.get('codes',[]):
                if code.get('language') == 'cpp' and 'text' in code:
                    assert any(normalized(code['text'],'cpp') in source for source in corpus), code['label']
                    count += 1
        print('Inline snippets',count,'current symbols and cumulative files passed')


if __name__ == '__main__':
    main()
