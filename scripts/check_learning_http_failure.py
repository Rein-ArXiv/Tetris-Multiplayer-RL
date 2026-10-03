"""Lesson119: retry disposition, pacing, I/O phase budgets and safe diagnostics."""
from pathlib import Path
import json,subprocess,tempfile,sys
from check_learning_text_layout import run
from check_learning_tables_keys import service
from check_learning_icon_ownership import settlement_contract
from check_part_docs import normalized
ROOT=Path(__file__).resolve().parents[1]
CP=ROOT/'docs/learn/checkpoints/119-http-failure'
OUT=ROOT/'out/learning-checkpoints/119-http-failure-check'

def root_contract():
    flags=['c++','-std=c++17','-pthread','-Wall','-Wextra','-Wpedantic','-I'+str(ROOT),'-isystem',str(ROOT/'third_party')]
    for name,src in [('current-http-failures','current_http_failures.cpp'),('match-http','match_http.cpp')]:
        exe=OUT/name
        run([*flags,'-DCPPHTTPLIB_OPENSSL_SUPPORT',str(ROOT/('tests/learning/'+src)),str(ROOT/'meta/http_client.cpp'),'-lssl','-lcrypto','-o',str(exe)],timeout=180)
        result=run([str(exe)],timeout=20)
        assert 'FAKE-CREDENTIAL-119' not in result.stderr and '[forged log line]' not in result.stderr
        if name=='current-http-failures':
            for marker in ['/v1/guest HTTP 400','/v1/auth/verify HTTP 500','/v1/matches HTTP 409','invalid URL']:
                assert marker in result.stderr,marker
        print(result.stdout,flush=True)
    print('Actual diagnostics: endpoint/status remain, synthetic credential echo/forged lines/input URL absent',flush=True)

def main():
    OUT.mkdir(parents=True,exist_ok=True)
    old=CP.parent/'118-json-boundary'
    changed={'README.md','DESIGN.md','CMakeLists.txt','meta/http_sender.h','net/match_submission.h','tools/meta_submit.cpp','tests/match_submission_contract.cpp'}
    for p in old.rglob('*'):
        if p.is_file() and p.relative_to(old).as_posix() not in changed:
            assert p.read_bytes()==(CP/p.relative_to(old)).read_bytes(),p
    if '--snippets-only' not in sys.argv:
        for backend in ['SCRIPTED','SDL']:
            build=OUT/backend.lower()
            run(['cmake','-S',str(CP),'-B',str(build),'-DSTUDY_PLATFORM='+backend,'-DCMAKE_BUILD_TYPE=Release','-DCMAKE_CXX_FLAGS=-Wall -Wextra -Wpedantic'])
            targets=['http_retry_contract','match_submission_contract','match_submission_probe','meta_service_contract','meta_submit']
            if backend=='SCRIPTED':targets+=['study_meta_db','settlement_probe']
            result=run(['cmake','--build',str(build),'--target',*targets,'-j2'])
            (OUT/('build-'+backend+'.log')).write_text(result.stdout+result.stderr)
            for line in (result.stdout+result.stderr).splitlines():
                if 'warning:' in line:assert '/third_party/sqlite3.c:' in line and any(x in line for x in ['[-Wdiscarded-qualifiers]','[-Wstringop-overread]']),line
            print(run(['ctest','--test-dir',str(build),'-R','^(http_retry_contract|match_submission_(contract|probe)|meta_service_contract)$','--output-on-failure'],timeout=120).stdout,flush=True)
            if backend=='SCRIPTED':
                settlement_contract(build)
                with tempfile.TemporaryDirectory(prefix='http-retry-db-') as tmp:
                    with service(build/'study_meta_db',Path(tmp)/'study.db') as port:
                        first=run([str(build/'meta_submit'),str(port),'119']).stdout
                        again=run([str(build/'meta_submit'),str(port),'119']).stdout
                        assert first==again and first.startswith('confirmed key=119 row=1'),(first,again)
                        print('Cumulative simulation -> retry driver -> HTTP -> SQLite: stable receipt repeated',flush=True)
        exe=OUT/'http-retry-sanitized'
        run(['c++','-std=c++17','-pthread','-Wall','-Wextra','-Wpedantic','-fsanitize=address,undefined','-fno-sanitize-recover=all','-I'+str(CP),'-isystem',str(ROOT/'third_party'),str(CP/'tests/http_retry_contract.cpp'),'-o',str(exe)],timeout=180)
        print(run([str(exe)],timeout=30).stdout,flush=True)
        root_contract()
    lesson=ROOT/'docs/learn/lessons/119.json'
    if lesson.exists():
        corpora={lang:[normalized(p.read_text(),lang) for p in CP.rglob('*') if p.is_file() and
            (p.suffix in ['.cpp','.h'] if lang=='cpp' else p.name=='CMakeLists.txt')] for lang in ['cpp','cmake']}
        n=0
        for section in json.loads(lesson.read_text())['sections']:
            for code in section.get('codes',[]):
                if 'text' in code and code['language'] in corpora:
                    assert any(normalized(code['text'],code['language']) in c for c in corpora[code['language']]),code['label'];n+=1
        print('inline snippets',n)
if __name__=='__main__':main()
