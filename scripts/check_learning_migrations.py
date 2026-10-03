"""Lesson112: atomic schema/data migration, restored headers and interrupted processes."""
from pathlib import Path
import json,re,sqlite3,subprocess,sys,tempfile,time
from concurrent.futures import ThreadPoolExecutor
from check_learning_text_layout import run
from check_learning_tables_keys import service
from check_learning_meta_service import request
from check_part_docs import normalized
ROOT=Path(__file__).resolve().parents[1]
CP=ROOT/'docs/learn/checkpoints/112-migrations'
OUT=ROOT/'out/learning-checkpoints/112-migrations-check'
BASE=re.search(r'R"sql\((.*?)\)sql";', (CP/'meta/schema.h').read_text(),re.S)[1]
def legacy(path):
    with sqlite3.connect(path) as db:
        db.executescript(BASE)
        db.executescript("INSERT INTO icons VALUES('ruby','Ruby'); INSERT INTO players VALUES('101','Keep A'),('202','Keep B'); "
            "INSERT INTO player_icons VALUES('101','ruby'); "
            "INSERT INTO matches(match_key,round,player_a,player_b,winner,ticks,score_a,score_b,lines_a,lines_b) "
            "VALUES('17','1','101','202','101','11','100','0',0,0);")
def contents(path):
    with sqlite3.connect(path) as db:
        return '\n'.join(db.iterdump())
def upgraded(path):
    with sqlite3.connect(path) as db:
        assert db.execute('PRAGMA user_version').fetchone()==(2,)
        assert db.execute('PRAGMA application_id').fetchone()==(1413829714,)
        assert db.execute('SELECT version,name FROM study_schema_history ORDER BY version').fetchall()==[(1,'base_tables_v1'),(2,'default_selection_v2')]
        assert db.execute('SELECT id,display_name FROM players ORDER BY id').fetchall()==[('101','Keep A'),('202','Keep B')]
        assert db.execute('SELECT id,match_key,score_a FROM matches').fetchall()==[(1,'17','100')]
        assert db.execute('PRAGMA foreign_key_check').fetchall()==[]
def process_contract(build):
    upgrade=build/'schema_upgrade'
    with tempfile.TemporaryDirectory(prefix='study-migration-112-') as tmp:
        tmp=Path(tmp);db=tmp/'legacy.db';legacy(db);before=contents(db)
        log=tmp/'interrupted.log'
        with log.open('w') as output:
            proc=subprocess.Popen([str(upgrade),str(db),'--pause-after-column'],stdin=subprocess.PIPE,stdout=output,stderr=output)
        try:
            deadline=time.monotonic()+8
            while 'COLUMN_PENDING' not in log.read_text():
                assert proc.poll() is None,log.read_text()
                assert time.monotonic()<deadline,'pause timeout'
                time.sleep(.02)
            proc.kill();proc.wait(timeout=3)
        finally:
            if proc.poll() is None:proc.kill();proc.wait(timeout=3)
            proc.stdin.close()
        assert contents(db)==before,'process death must recover the old logical DB'
        assert run([str(upgrade),str(db)]).stdout.strip()=='schema_version=2'
        upgraded(db)
        with sqlite3.connect(db) as conn:conn.execute("UPDATE players SET selected_icon_id='ruby' WHERE id='101'")
        record=dict(key=17,round=1,player_a=101,player_b=202,ticks=11,score_a=100,score_b=0,lines_a=0,lines_b=0,winner=1)
        with service(build/'study_meta_db',db) as port:
            status,receipt=request(port,record);assert status==200 and receipt['row']==1
        upgraded(db)
        with sqlite3.connect(db) as src:
            dump='\n'.join(src.iterdump())
            backup=tmp/'backup.db'
            with sqlite3.connect(backup) as dest:src.backup(dest)
        restored=tmp/'restored.db'
        with sqlite3.connect(restored) as conn:
            conn.executescript(dump)
            assert conn.execute('PRAGMA user_version').fetchone()==(0,)
            assert conn.execute('PRAGMA application_id').fetchone()==(0,)
        for p in [backup,restored]:
            run([str(upgrade),str(p)]);upgraded(p)
            with sqlite3.connect(p) as conn:assert conn.execute("SELECT selected_icon_id FROM players WHERE id='101'").fetchone()==('ruby',)
        race=tmp/'race.db';legacy(race)
        with ThreadPoolExecutor(max_workers=2) as executor:
            outputs=list(executor.map(lambda _:run([str(upgrade),str(race)]).stdout,range(2)))
        assert outputs==['schema_version=2\n']*2;upgraded(race)
        unrelated=tmp/'unrelated.db'
        with sqlite3.connect(unrelated) as conn:conn.executescript("CREATE TABLE unrelated(note TEXT);INSERT INTO unrelated VALUES('keep');")
        before=contents(unrelated)
        p=subprocess.run([str(upgrade),str(unrelated)],capture_output=True,text=True,timeout=5)
        assert p.returncode==1 and contents(unrelated)==before
    print('Interrupted-process rollback, same receipt, chosen-icon preservation, SQL dump/binary backup, concurrent migration and foreign DB rejection: passed',flush=True)
def main():
    OUT.mkdir(parents=True,exist_ok=True)
    old=CP.parent/'111-indexes'
    for p in old.rglob('*'):
        rel=p.relative_to(old)
        if p.is_file() and str(rel) not in {'README.md','DESIGN.md','CMakeLists.txt','meta/sqlite_results.h'}:
            assert p.read_bytes()==(CP/rel).read_bytes(),rel
    if '--snippets-only' not in sys.argv:
        for backend in ['SCRIPTED','SDL']:
            b=OUT/backend.lower()
            run(['cmake','-S',str(CP),'-B',str(b),'-DSTUDY_PLATFORM='+backend,'-DCMAKE_BUILD_TYPE=Release','-DCMAKE_CXX_FLAGS=-Wall -Wextra -Wpedantic'])
            result=run(['cmake','--build',str(b),'--target','study_meta_db','sqlite_tables_contract','index_probe','schema_upgrade','migration_contract','-j2'])
            (OUT/('build-'+backend+'.log')).write_text(result.stdout+result.stderr)
            warnings=[x for x in (result.stdout+result.stderr).splitlines() if 'warning:' in x]
            for w in warnings:assert '/third_party/sqlite3.c:' in w and any(t in w for t in ['[-Wdiscarded-qualifiers]','[-Wstringop-overread]']),w
            if warnings:print('Vendored SQLite diagnostics:',len(warnings),flush=True)
            print(run(['ctest','--test-dir',str(b),'-R','^(sqlite_tables_contract|index_query_contract)$','--output-on-failure']).stdout,flush=True)
            with tempfile.TemporaryDirectory(prefix='migration-contract-') as tmp:
                print(run([str(b/'migration_contract'),str(Path(tmp)/'test.db')]).stdout,flush=True)
            process_contract(b)
        exe=OUT/'migration-sanitized'
        run(['c++','-std=c++17','-pthread','-Wall','-Wextra','-Wpedantic','-fsanitize=address,undefined','-fno-sanitize-recover=all',
             '-I'+str(CP),'-isystem',str(ROOT/'third_party'),str(CP/'tests/migration_contract.cpp'),str(OUT/'scripted/libstudy_sqlite.a'),'-ldl','-o',str(exe)])
        with tempfile.TemporaryDirectory(prefix='migration-asan-') as tmp:print(run([str(exe),str(Path(tmp)/'test.db')]).stdout,flush=True)
    p=ROOT/'docs/learn/lessons/112.json'
    if p.exists():
        corpora={lang:[normalized(p.read_text(),lang) for p in CP.rglob('*') if p.is_file() and
            (p.suffix in ['.cpp','.h'] if lang in ['cpp','sql'] else p.name=='CMakeLists.txt')] for lang in ['cpp','sql','cmake']}
        count=0
        for section in json.loads(p.read_text())['sections']:
            for c in section.get('codes',[]):
                if c['language'] in corpora and 'text' in c:
                    assert any(normalized(c['text'],c['language']) in s for s in corpora[c['language']]),c['label']
                    count+=1
        print('inline snippets:',count)
if __name__=='__main__':main()
