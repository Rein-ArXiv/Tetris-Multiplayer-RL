"""Lesson114: immutable settlement receipts and duplicate input identity."""
from pathlib import Path
import json,subprocess,tempfile,sys,urllib.request,urllib.error
from concurrent.futures import ThreadPoolExecutor
from check_learning_text_layout import run
from check_learning_tables_keys import service
from check_learning_transactions import process_contract
from check_learning_migrations import legacy
from check_part_docs import normalized
ROOT=Path(__file__).resolve().parents[1]
CP=ROOT/'docs/learn/checkpoints/114-idempotency'
OUT=ROOT/'out/learning-checkpoints/114-idempotency-check'

def post(port,record,version=2):
    body=record if isinstance(record,str) else json.dumps(record)
    req=urllib.request.Request(f'http://127.0.0.1:{port}/study/v{version}/matches',body.encode(),{'Content-Type':'application/json'})
    try:
        with urllib.request.urlopen(req,timeout=8) as response:return response.status,json.loads(response.read())
    except urllib.error.HTTPError as e:return e.code,json.loads(e.read())

def settlement_contract(build):
    with tempfile.TemporaryDirectory(prefix='settlement-114-') as tmp:
        db=Path(tmp)/'study.db'
        record=dict(key=17,round=1,player_a=101,player_b=202,ticks=11,score_a=100,score_b=0,lines_a=0,lines_b=0,winner=1)
        with service(build/'study_meta_db',db) as port:
            first=post(port,record);assert first[0]==200 and first[1]['bp_a']==10 and first[1]['bp_b']==3
            assert post(port,dict(record,key=18))[0]==200
            assert post(port,json.dumps(dict(reversed(list(record.items()))),indent=4))==first
            for field in record:
                if field=='key':continue
                changed=dict(record);changed[field]+=1;assert post(port,changed)[0]==409,field
            with ThreadPoolExecutor(max_workers=8) as pool:assert list(pool.map(lambda _:post(port,record),range(8)))==[first]*8
            expected={k:v for k,v in first[1].items() if k not in ['bp_a','bp_b','policy']}
            assert post(port,record,1)==(200,expected)
            assert run([str(build/'settlement_probe'),str(port),'17']).stdout=='row=1 awards=10,3 policy=1\n'
            wide=dict(record,key=2**64-1);assert post(port,wide)[0]==200
            assert run([str(build/'settlement_probe'),str(port),str(2**64-1)]).stdout.endswith('awards=10,3 policy=1\n')
        with service(build/'study_meta_db',db) as port:assert post(port,record)==first
        old=Path(tmp)/'old.db';legacy(old)
        with service(build/'study_meta_db',old) as port:
            old_result=post(port,record);assert old_result[0]==200
            assert (old_result[1]['policy'],old_result[1]['bp_a'],old_result[1]['bp_b'])==(0,0,0)
    print('HTTP v1 compatibility/v2 original awards, semantic JSON, all-field conflicts, concurrency, UINT64 and restart/legacy: passed',flush=True)

def root_contract(archive):
    exe=OUT/'current-idempotency'
    run(['c++','-std=c++17','-pthread','-Wall','-Wextra','-Wpedantic','-fsanitize=address,undefined','-fno-sanitize-recover=all',
         '-I'+str(ROOT),'-I'+str(CP),'-isystem',str(ROOT/'third_party'),str(ROOT/'tests/learning/current_idempotency.cpp'),
         str(ROOT/'meta/credentials.cpp'),str(archive),'-lcrypto','-ldl','-o',str(exe)])
    with tempfile.TemporaryDirectory(prefix='root-idempotency-') as tmp:print(run([str(exe),str(Path(tmp)/'fixture-')]).stdout,flush=True)

def main():
    OUT.mkdir(parents=True,exist_ok=True)
    old=CP.parent/'113-transactions'
    changed={'README.md','DESIGN.md','CMakeLists.txt','meta/sqlite_results.h','meta/database_service.cpp'}
    for p in old.rglob('*'):
        if p.is_file() and p.relative_to(old).as_posix() not in changed:assert p.read_bytes()==(CP/p.relative_to(old)).read_bytes(),p
    if '--snippets-only' not in sys.argv:
        for backend in ['SCRIPTED','SDL']:
            build=OUT/backend.lower()
            run(['cmake','-S',str(CP),'-B',str(build),'-DSTUDY_PLATFORM='+backend,'-DCMAKE_BUILD_TYPE=Release','-DCMAKE_CXX_FLAGS=-Wall -Wextra -Wpedantic'])
            result=run(['cmake','--build',str(build),'--target','study_meta_db','sqlite_tables_contract','index_probe','migration_contract','reward_contract','reward_probe','idempotency_contract','settlement_probe','-j2'])
            (OUT/('build-'+backend+'.log')).write_text(result.stdout+result.stderr)
            for line in (result.stdout+result.stderr).splitlines():
                if 'warning:' in line:assert '/third_party/sqlite3.c:' in line and any(x in line for x in ['[-Wdiscarded-qualifiers]','[-Wstringop-overread]']),line
            print(run(['ctest','--test-dir',str(build),'-R','^(sqlite_tables_contract|index_query_contract)$','--output-on-failure']).stdout,flush=True)
            for name in ['migration_contract','reward_contract','idempotency_contract']:
                with tempfile.TemporaryDirectory(prefix='idempotency-contract-') as tmp:print(run([str(build/name),str(Path(tmp)/'fixture.db')]).stdout,flush=True)
            process_contract(build);settlement_contract(build)
        exe=OUT/'idempotency-sanitized';archive=OUT/'scripted/libstudy_sqlite.a'
        run(['c++','-std=c++17','-pthread','-Wall','-Wextra','-Wpedantic','-fsanitize=address,undefined','-fno-sanitize-recover=all',
             '-I'+str(CP),'-isystem',str(ROOT/'third_party'),str(CP/'tests/idempotency_contract.cpp'),str(archive),'-ldl','-o',str(exe)])
        with tempfile.TemporaryDirectory(prefix='idempotency-asan-') as tmp:print(run([str(exe),str(Path(tmp)/'test.db')]).stdout,flush=True)
        root_contract(archive)
    lesson=ROOT/'docs/learn/lessons/114.json'
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
