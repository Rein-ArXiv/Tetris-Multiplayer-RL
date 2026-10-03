"""Check lesson107's connection/phase/byte leases and root allocation rollback."""
from pathlib import Path
import sys,json
from check_learning_text_layout import run
from check_part_docs import normalized
ROOT=Path(__file__).resolve().parents[1]
CP=ROOT/'docs/learn/checkpoints/107-connection-budget'
OUT=ROOT/'out/learning-checkpoints/107-connection-budget-check'
def check_root():
    OUT.mkdir(parents=True,exist_ok=True)
    flags=['c++','-std=c++17','-pthread','-Wall','-Wextra','-Wpedantic','-I'+str(ROOT)]
    allocation=OUT/'allocation'
    run([*flags,str(ROOT/'tests/learning/admission_allocation.cpp'),'-o',str(allocation)])
    for mode in ['fresh','existing','player']:
        for index in range(7):
            # A destructor trying to relock its acquisition mutex must fail by timeout.
            result=run([str(allocation),mode,str(index)],timeout=3)
            print(result.stdout,flush=True)
    contract=OUT/'root-contract'
    run([*flags,'-fsanitize=address,undefined','-fno-sanitize-recover=all',
         str(ROOT/'tests/learning/admission_contract.cpp'),'-o',str(contract)])
    print(run([str(contract)],timeout=15).stdout,flush=True)

def main():
    OUT.mkdir(parents=True,exist_ok=True)
    if "--skip-root" not in sys.argv: check_root()
    old=CP.parent/'106-room-exit'
    for p in old.rglob('*'):
        rel=p.relative_to(old)
        if p.is_file() and rel.as_posix() not in {'README.md','DESIGN.md','CMakeLists.txt'}:
            assert p.read_bytes()==(CP/rel).read_bytes(),rel
    flags=['c++','-std=c++17','-pthread','-Wall','-Wextra','-Wpedantic',
           '-fsanitize=address,undefined','-fno-sanitize-recover=all','-I'+str(CP)]
    for name,source,extra in [
        ('contract','tests/connection_budget_contract.cpp',[]),
        ('probe','tools/connection_budget_probe.cpp',
         [str(CP/('net/'+x+'.cpp')) for x in ['socket','stream','send_socket','receive_socket']])]:
        exe=OUT/name
        run([*flags,str(CP/source),*extra,'-o',str(exe)])
        p=run([str(exe)],timeout=20);print(p.stdout,p.stderr,flush=True)
    if '--skip-build' not in sys.argv:
        for backend in ['SCRIPTED','SDL']:
            b=OUT/backend.lower()
            run(['cmake','-S',str(CP),'-B',str(b),'-DSTUDY_PLATFORM='+backend,
                 '-DCMAKE_BUILD_TYPE=Release','-DCMAKE_CXX_FLAGS=-Wall -Wextra -Wpedantic'])
            p=run(['cmake','--build',str(b),'--target','connection_budget_contract','connection_budget_probe','-j3'])
            (OUT/('build-'+backend+'.log')).write_text(p.stdout+p.stderr)
            assert 'warning:' not in p.stdout+p.stderr, p.stdout+p.stderr
            p=run(['ctest','--test-dir',str(b),'-R','^connection_budget_(contract|probe)$','--output-on-failure'])
            print(backend,p.stdout,flush=True)
    lesson=ROOT/'docs/learn/lessons/107.json'
    if lesson.exists():
        corpora={lang:[normalized(p.read_text(),lang) for p in CP.rglob('*') if p.is_file() and
                       (p.suffix in ['.cpp','.h'] if lang=='cpp' else p.name=='CMakeLists.txt')]
                 for lang in ['cpp','cmake']}
        count=0
        for sec in json.loads(lesson.read_text())['sections']:
            for code in sec.get('codes',[]):
                if code['language'] in corpora and 'text' in code:
                    assert any(normalized(code['text'],code['language']) in text for text in corpora[code['language']]),code['label']
                    count+=1
        print('inline snippets:',count)
if __name__=='__main__':main()
