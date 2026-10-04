"""Lesson167: actual cumulative roles, isolated dependency graphs and current builds."""
from pathlib import Path
import argparse
import json
import subprocess
import sys
import tempfile
from check_learning_text_layout import run
from check_part_docs import normalized

R=Path(__file__).resolve().parents[1]
CP=R/'docs/learn/checkpoints/167-build-targets'
OUT=R/'out/learning-checkpoints/167-build-targets-check'


def graph(build):
    reply=build/'.cmake/api/v1/reply'
    index=max(reply.glob('index-*.json'),key=lambda p:p.stat().st_mtime_ns)
    model=json.loads((reply/json.loads(index.read_text())['reply']['codemodel-v2']['jsonFile']).read_text())
    return {t['name'] for c in model['configurations'] for t in c['targets']}


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--python',default=sys.executable)
    parser.add_argument('--snippets-only',action='store_true')
    args=parser.parse_args()
    previous=CP.with_name('166-policy-fallback')
    for path in previous.rglob('*'):
        if path.is_file() and '__pycache__' not in path.parts and path.relative_to(previous).as_posix()!='README.md':
            assert path.read_bytes()==(CP/path.relative_to(previous)).read_bytes(),path
    OUT.mkdir(parents=True,exist_ok=True)
    if not args.snippets_only:
        bad=subprocess.run(['cmake','-S',str(CP/'roles'),'-B',str(OUT/'invalid'),'-DSTUDY_ROLE=UNKNOWN'],text=True,capture_output=True)
        assert bad.returncode and 'Unknown STUDY_ROLE' in bad.stderr
        pybind=run([args.python,'-m','pybind11','--cmakedir']).stdout.strip()
        expected={'RULES':{'binding_oracle','policy_match_contract'},'SERVICE':{'study_account_db','account_contract'},
                  'TRAINING':{'study_py','binding_oracle','policy_match_contract'},'POLICY':{'study_policy','policy_match_probe'}}
        products={'study_account_db','study_py','study_policy'}
        for role in expected:
            build=OUT/role.lower();query=build/'.cmake/api/v1/query';query.mkdir(parents=True,exist_ok=True)
            (query/'codemodel-v2').touch()
            command=['cmake','-S',str(CP/'roles'),'-B',str(build),'-DSTUDY_ROLE='+role,'-DCMAKE_BUILD_TYPE=Release']
            if role=='RULES':
                command+=['-DCMAKE_DISABLE_FIND_PACKAGE_OpenSSL=TRUE','-DCMAKE_DISABLE_FIND_PACKAGE_Python=TRUE',
                          '-DCMAKE_DISABLE_FIND_PACKAGE_pybind11=TRUE','-DCMAKE_DISABLE_FIND_PACKAGE_SDL2=TRUE',
                          '-DSTUDY_VENDOR_DIR='+str(OUT/'missing-vendor'),'-DORT_ROOT='+str(OUT/'missing-ort')]
            elif role=='TRAINING':command+=['-DPython_EXECUTABLE='+args.python,'-Dpybind11_DIR='+pybind]
            elif role=='POLICY':command+=['-DORT_ROOT='+str(R/'third_party/onnxruntime')]
            print(role,run(command).stdout[-350:],flush=True)
            targets=graph(build);assert expected[role]<=targets
            assert targets&products==expected[role]&products,(role,targets&products)
            result=run(['cmake','--build',str(build),'--config','Release','-j2'],timeout=300)
            (OUT/(role.lower()+'-build.log')).write_text(result.stdout+result.stderr)
            if role=='SERVICE':
                with tempfile.TemporaryDirectory(prefix='study-role-db-') as tmp:
                    print(run([str(build/'account_contract'),str(Path(tmp)/'contract.db')]).stdout)
            else:
                print(run(['ctest','--test-dir',str(build),'-C','Release','--output-on-failure'],timeout=300).stdout,flush=True)
        print(run([args.python,str(CP/'tests/policy_match_inference.py'),
                   '--module-dir',str(OUT/'training/bindings/python/Release'),
                   '--probe',str(OUT/'policy/inference/policy_match_probe')],timeout=180).stdout)
        # This service owns actual HTTP/SQLite state. Run the optional socket audit separately.
        oracle=run([str(OUT/'rules/binding_oracle'),'77','48','0','16']).stdout
        bound=run([str(OUT/'training/bindings/binding_oracle'),'77','48','0','16']).stdout
        assert oracle==bound
        print('Role graphs, rules bytes, cumulative native contracts and real policy parity passed')
    manuscript=R/'docs/learn/lessons/167.json'
    if manuscript.exists():
        corpus={language:[normalized(p.read_text(),language) for p in CP.rglob('*')
                         if p.is_file() and (p.suffix in exts or p.name in names)]
                for language,exts,names in [('cpp',{'.h','.cpp'},set()),('cmake',{'.cmake'},{'CMakeLists.txt'})]}
        count=0
        for section in json.loads(manuscript.read_text())['sections']:
            ref=section.get('reference')
            if ref:assert ref['symbol'] in (R/ref['path']).read_text(),ref
            for code in section.get('codes',[]):
                language=code.get('language')
                if language in corpus and 'text' in code:
                    assert any(normalized(code['text'],language) in source for source in corpus[language]),code['label']
                    count+=1
        print('Inline snippets',count,'current references and cumulative preservation passed')

if __name__=='__main__':main()
