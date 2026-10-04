"""Lesson164: pacing gates, real empty ticks and CPU ONNX frame integration."""
from pathlib import Path
import argparse
import json
import sys
from check_learning_text_layout import run
from check_part_docs import normalized

R=Path(__file__).resolve().parents[1]
CP=R/'docs/learn/checkpoints/164-bot-pacing'
OUT=R/'out/learning-checkpoints/164-bot-pacing-check'


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--python',default=sys.executable)
    parser.add_argument('--snippets-only',action='store_true')
    args=parser.parse_args()
    changed={'README.md','bindings/CMakeLists.txt','bindings/session.h','inference/CMakeLists.txt'}
    previous=CP.with_name('163-input-route')
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
        print(run([str(build/'paced_demo')],timeout=30).stdout)
        inference=OUT/'inference'
        run(['cmake','-S',str(CP/'inference'),'-B',str(inference),'-DCMAKE_BUILD_TYPE=Release',
             '-DORT_ROOT='+str(R/'third_party/onnxruntime'),'-DCMAKE_CXX_FLAGS=-Wall -Wextra -Wpedantic'])
        print(run(['cmake','--build',str(inference),'--target','onnx_paced_probe','--config','Release','-j2'],timeout=180).stdout)
        print(run([args.python,str(CP/'tests/paced_inference.py'),
                   '--module-dir',str(build/'python/Release'),'--probe',str(inference/'onnx_paced_probe')],timeout=180).stdout)
    path=R/'docs/learn/lessons/164.json'
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
