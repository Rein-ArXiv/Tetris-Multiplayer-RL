"""Lesson113: commit a result, two ledger entries and balances as one unit."""
from pathlib import Path
import json,sqlite3,subprocess,tempfile,time,sys
from concurrent.futures import ThreadPoolExecutor
from check_learning_text_layout import run
from check_learning_tables_keys import service
from check_learning_meta_service import request
from check_learning_migrations import legacy
from check_part_docs import normalized
ROOT=Path(__file__).resolve().parents[1]
CP=ROOT/'docs/learn/checkpoints/113-transactions'
OUT=ROOT/'out/learning-checkpoints/113-transactions-check'

def snapshot(path):
    with sqlite3.connect(path) as db:
        return (db.execute('SELECT count(*) FROM matches').fetchone()[0],
                db.execute('SELECT player_id,bp FROM wallets ORDER BY player_id').fetchall(),
                db.execute('SELECT count(*) FROM match_rewards').fetchone()[0])

def process_contract(build):
    with tempfile.TemporaryDirectory(prefix='study-rewards-113-') as tmp:
        tmp=Path(tmp)
        for mode,message,expected in [('pause-before','PENDING',(0,[('101',0),('202',0)],0)),
                                      ('pause-after','COMMITTED',(1,[('101',10),('202',3)],2))]:
            db=tmp/(mode+'.db');log=tmp/(mode+'.log')
            with log.open('w') as out:
                p=subprocess.Popen([str(build/'reward_probe'),str(db),'17',mode],stdin=subprocess.PIPE,stdout=out,stderr=out)
            try:
                deadline=time.monotonic()+8
                while message not in log.read_text():
                    assert p.poll() is None,log.read_text()
                    assert time.monotonic()<deadline,'pause timeout'
                    time.sleep(.02)
                assert snapshot(db)==expected
                p.kill();p.wait(timeout=3)
            finally:
                if p.poll() is None:p.kill();p.wait(timeout=3)
                p.stdin.close()
            assert snapshot(db)==expected
            result=run([str(build/'reward_probe'),str(db),'17']).stdout
            assert result=='row=1 awards=10,3 balances=10,3\n',result
            assert snapshot(db)==(1,[('101',10),('202',3)],2)
        db=tmp/'legacy.db';legacy(db)
        # Old recorded matches get explicit zero-policy entries, never retroactive awards.
        with service(build/'study_meta_db',db) as port:
            record=dict(key=17,round=1,player_a=101,player_b=202,ticks=11,score_a=100,score_b=0,lines_a=0,lines_b=0,winner=1)
            assert request(port,record)[0]==200
            assert snapshot(db)==(1,[('101',0),('202',0)],2)
            with sqlite3.connect(db) as q:assert q.execute('SELECT DISTINCT policy,bp_delta FROM match_rewards').fetchall()==[(0,0)]
            new=dict(record,key=18)
            with ThreadPoolExecutor(max_workers=8) as pool:results=list(pool.map(lambda _:request(port,new),range(8)))
            assert all(r==results[0] and r[0]==200 for r in results)
            assert snapshot(db)==(2,[('101',10),('202',3)],4)
            assert request(port,dict(new,score_a=101))[0]==409
        with service(build/'study_meta_db',db) as port:
            assert request(port,new)==results[0]
            assert snapshot(db)==(2,[('101',10),('202',3)],4)
    print('Process death before/after commit, old-data zero awards, HTTP retry/conflict and restart: passed',flush=True)

def main():
    OUT.mkdir(parents=True,exist_ok=True)
    old=CP.parent/'112-migrations'
    changed={'README.md','DESIGN.md','CMakeLists.txt','meta/migrations.h','meta/sqlite_results.h','tests/migration_contract.cpp'}
    for p in old.rglob('*'):
        if p.is_file() and p.relative_to(old).as_posix() not in changed:
            assert p.read_bytes()==(CP/p.relative_to(old)).read_bytes(),p
    if '--snippets-only' not in sys.argv:
        for backend in ['SCRIPTED','SDL']:
            build=OUT/backend.lower()
            run(['cmake','-S',str(CP),'-B',str(build),'-DSTUDY_PLATFORM='+backend,'-DCMAKE_BUILD_TYPE=Release','-DCMAKE_CXX_FLAGS=-Wall -Wextra -Wpedantic'])
            result=run(['cmake','--build',str(build),'--target','study_meta_db','sqlite_tables_contract','index_probe','migration_contract','reward_contract','reward_probe','-j2'])
            (OUT/('build-'+backend+'.log')).write_text(result.stdout+result.stderr)
            for line in (result.stdout+result.stderr).splitlines():
                if 'warning:' in line:assert '/third_party/sqlite3.c:' in line and any(x in line for x in ['[-Wdiscarded-qualifiers]','[-Wstringop-overread]']),line
            print(run(['ctest','--test-dir',str(build),'-R','^(sqlite_tables_contract|index_query_contract)$','--output-on-failure']).stdout,flush=True)
            for name in ['migration_contract','reward_contract']:
                with tempfile.TemporaryDirectory(prefix='reward-contract-') as tmp:print(run([str(build/name),str(Path(tmp)/'fixture.db')]).stdout,flush=True)
            process_contract(build)
        exe=OUT/'reward-sanitized'
        run(['c++','-std=c++17','-pthread','-Wall','-Wextra','-Wpedantic','-fsanitize=address,undefined','-fno-sanitize-recover=all',
             '-I'+str(CP),'-isystem',str(ROOT/'third_party'),str(CP/'tests/reward_contract.cpp'),str(OUT/'scripted/libstudy_sqlite.a'),'-ldl','-o',str(exe)])
        with tempfile.TemporaryDirectory(prefix='reward-asan-') as tmp:print(run([str(exe),str(Path(tmp)/'test.db')]).stdout,flush=True)
    lesson=ROOT/'docs/learn/lessons/113.json'
    if lesson.exists():
        corpora={lang:[normalized(p.read_text(),lang) for p in CP.rglob('*') if p.is_file() and
            (p.suffix in ['.cpp','.h'] if lang in ['cpp','sql'] else p.name=='CMakeLists.txt')] for lang in ['cpp','sql','cmake']}
        n=0
        for section in json.loads(lesson.read_text())['sections']:
            for code in section.get('codes',[]):
                if 'text' in code and code['language'] in corpora:
                    assert any(normalized(code['text'],code['language']) in c for c in corpora[code['language']]),code['label'];n+=1
        print('inline snippets',n)
if __name__=='__main__':main()
