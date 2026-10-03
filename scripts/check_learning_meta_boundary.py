"""Validate lesson108's immutable submission and actual match response parser."""
from pathlib import Path
import sys,json
from check_learning_text_layout import run
from check_part_docs import normalized
ROOT=Path(__file__).resolve().parents[1]
CP=ROOT/'docs/learn/checkpoints/108-meta-boundary'
OUT=ROOT/'out/learning-checkpoints/108-meta-boundary-check'
def check_root():
    flags=['c++','-std=c++17','-pthread','-Wall','-Wextra','-Wpedantic','-I'+str(ROOT),'-I'+str(ROOT/'third_party')]
    wrapper=OUT/'response-after.cpp'
    wrapper.write_text('#include "meta/match_response.h"\nstd::optional<meta::client::MatchResult> parse_response(const std::string& body){return meta::client::parse_match_response(body);}\n')
    exe=OUT/'response-after'
    run([*flags,'-fsanitize=address,undefined','-fno-sanitize-recover=all',str(ROOT/'tests/learning/match_response.cpp'),str(wrapper),'-o',str(exe)])
    print(run([str(exe)],timeout=10).stdout,flush=True)
    exe=OUT/'http'
    run([*flags,str(ROOT/'tests/learning/match_http.cpp'),str(ROOT/'meta/http_client.cpp'),'-o',str(exe)],timeout=180)
    result=run([str(exe)],timeout=15);print(result.stdout,result.stderr,flush=True)
def main():
    OUT.mkdir(parents=True,exist_ok=True)
    if '--skip-root' not in sys.argv:check_root()
    old=CP.parent/'107-connection-budget'
    for p in old.rglob('*'):
        rel=p.relative_to(old)
        if p.is_file() and rel.as_posix() not in {'README.md','DESIGN.md','CMakeLists.txt'}:
            assert p.read_bytes()==(CP/rel).read_bytes(),rel
    flags=['c++','-std=c++17','-pthread','-Wall','-Wextra','-Wpedantic','-fsanitize=address,undefined','-fno-sanitize-recover=all','-I'+str(CP)]
    for name,source,extra in [('contract','tests/match_submission_contract.cpp',[]),
        ('probe','tools/match_submission_probe.cpp',[str(CP/('net/'+x+'.cpp')) for x in ['socket','stream','send_socket','receive_socket']])]:
        exe=OUT/name;run([*flags,str(CP/source),*extra,'-o',str(exe)])
        p=run([str(exe)],timeout=20);print(p.stdout,p.stderr,flush=True)
    if '--skip-build' not in sys.argv:
        for backend in ['SCRIPTED','SDL']:
            b=OUT/backend.lower()
            run(['cmake','-S',str(CP),'-B',str(b),'-DSTUDY_PLATFORM='+backend,'-DCMAKE_BUILD_TYPE=Release','-DCMAKE_CXX_FLAGS=-Wall -Wextra -Wpedantic'])
            p=run(['cmake','--build',str(b),'--target','match_submission_contract','match_submission_probe','-j3'])
            (OUT/('build-'+backend+'.log')).write_text(p.stdout+p.stderr)
            assert 'warning:' not in p.stdout+p.stderr,p.stdout+p.stderr
            print(backend,run(['ctest','--test-dir',str(b),'-R','^match_submission_(contract|probe)$','--output-on-failure']).stdout,flush=True)
    lesson=ROOT/'docs/learn/lessons/108.json'
    if lesson.exists():
        corpora={lang:[normalized(p.read_text(),lang) for p in CP.rglob('*') if p.is_file() and
            (p.suffix in ['.cpp','.h'] if lang=='cpp' else p.name=='CMakeLists.txt')] for lang in ['cpp','cmake']}
        count=0
        for section in json.loads(lesson.read_text())['sections']:
            for code in section.get('codes',[]):
                if code['language'] in corpora and 'text' in code:
                    assert any(normalized(code['text'],code['language']) in text for text in corpora[code['language']]),code['label']
                    count+=1
        print('inline snippets:',count)
if __name__=='__main__':main()
