"""Lesson161: export graph structure, fixed I/O, actual tensor and action parity."""
from pathlib import Path
import argparse
import json
import sys
from check_learning_text_layout import run
from check_part_docs import normalized

R=Path(__file__).resolve().parents[1]
CP=R/'docs/learn/checkpoints/161-onnx-export'
OUT=R/'out/learning-checkpoints/161-onnx-export-check'


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--python',default=sys.executable)
    parser.add_argument('--snippets-only',action='store_true')
    args=parser.parse_args()
    changed={'README.md','bindings/CMakeLists.txt'}
    previous=CP.with_name('160-model-zoo')
    for path in previous.rglob('*'):
        if (path.is_file() and "__pycache__" not in path.parts
                and path.relative_to(previous).as_posix() not in changed):
            assert path.read_bytes()==(CP/path.relative_to(previous)).read_bytes(),path
    OUT.mkdir(parents=True,exist_ok=True)
    if not args.snippets_only:
        pybind=run([args.python,'-m','pybind11','--cmakedir']).stdout.strip()
        build=OUT/'native'
        run(['cmake','-S',str(CP/'bindings'),'-B',str(build),'-DCMAKE_BUILD_TYPE=Release',
             '-DPython_EXECUTABLE='+args.python,'-Dpybind11_DIR='+pybind,
             '-DCMAKE_CXX_FLAGS=-Wall -Wextra -Wpedantic'])
        result=run(['cmake','--build',str(build),'--config','Release','-j2'],timeout=180)
        (OUT/'build.log').write_text(result.stdout+result.stderr)
        print(run(['ctest','--test-dir',str(build),'-C','Release','--output-on-failure'],timeout=300).stdout)
    path=R/'docs/learn/lessons/161.json'
    if path.exists():
        corpus={language:[normalized(p.read_text(),language) for p in CP.rglob('*')
                         if p.is_file() and p.suffix in extensions]
                for language,extensions in [('cpp',{'.h','.cpp'}),('python',{'.py'})]}
        count=0
        for section in json.loads(path.read_text())['sections']:
            ref=section.get('reference')
            if ref:assert ref['symbol'] in (R/ref['path']).read_text(),ref
            for code in section.get('codes',[]):
                language=code.get('language')
                if language in corpus and 'text' in code:
                    assert any(normalized(code['text'],language) in source for source in corpus[language]),code['label']
                    count+=1
        print('Inline snippets',count,'current references and cumulative preservation passed')


if __name__=='__main__':main()
