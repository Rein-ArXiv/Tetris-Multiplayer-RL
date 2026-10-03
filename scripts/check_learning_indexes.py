"""Verify lesson111 query results, observed plans and persistent history integration."""
from pathlib import Path
import json,sys,tempfile,sqlite3
from check_learning_text_layout import run
from check_learning_tables_keys import service
from check_learning_meta_service import request
from check_part_docs import normalized
ROOT=Path(__file__).resolve().parents[1]
CP=ROOT/'docs/learn/checkpoints/111-indexes'
OUT=ROOT/'out/learning-checkpoints/111-indexes-check'
def process_contract(build):
    with tempfile.TemporaryDirectory(prefix='study-history-111-') as tmp:
        db=Path(tmp)/'history.db'
        record=dict(key=2**64-1,round=1,player_a=101,player_b=202,ticks=11,score_a=0,score_b=0,lines_a=0,lines_b=0,winner=0)
        with service(build/'study_meta_db',db) as port:
            first=request(port,record);assert first[0]==200 and first[1]['row']==1
            second=request(port,dict(record,key=3,player_a=202,player_b=101));assert second[0]==200 and second[1]['row']==2
        expected=f'row=2 key=3\nrow=1 key={2**64-1}\n'
        for player in ['101','202']:
            assert run([str(build/'history_list'),str(db),player,'5']).stdout==expected
        assert run([str(build/'history_list'),str(db),'101','1']).stdout=='row=2 key=3\n'
        # Simulate a valid110 database: no new performance indexes, same rows/schema.
        with sqlite3.connect(db) as conn:
            conn.execute('DROP INDEX idx_study_matches_a_recent');conn.execute('DROP INDEX idx_study_matches_b_recent')
        with service(build/'study_meta_db',db) as port:
            assert request(port,record)==first
        assert run([str(build/'history_list'),str(db),'101','5']).stdout==expected
        with sqlite3.connect(db) as conn:
            assert conn.execute('SELECT count(*) FROM matches').fetchone()==(2,)
            assert len(conn.execute("SELECT name FROM sqlite_schema WHERE type='index' AND name LIKE 'idx_study_matches_%_recent'").fetchall())==2
    print('HTTP storage, bounded two-sided history, restart and index creation on existing DB: passed',flush=True)
def main():
    OUT.mkdir(parents=True,exist_ok=True)
    old=CP.parent/'110-tables-keys'
    for p in old.rglob('*'):
        rel=p.relative_to(old)
        if p.is_file() and str(rel) not in {'README.md','DESIGN.md','CMakeLists.txt','meta/sqlite_results.h'}:
            assert p.read_bytes()==(CP/rel).read_bytes(),rel
    if '--snippets-only' not in sys.argv:
        for backend in ['SCRIPTED','SDL']:
            b=OUT/backend.lower()
            run(['cmake','-S',str(CP),'-B',str(b),'-DSTUDY_PLATFORM='+backend,'-DCMAKE_BUILD_TYPE=Release','-DCMAKE_CXX_FLAGS=-Wall -Wextra -Wpedantic'])
            result=run(['cmake','--build',str(b),'--target','study_meta_db','sqlite_tables_contract','index_probe','history_list','-j2'])
            (OUT/('build-'+backend+'.log')).write_text(result.stdout+result.stderr)
            warnings=[x for x in (result.stdout+result.stderr).splitlines() if 'warning:' in x]
            for w in warnings:assert '/third_party/sqlite3.c:' in w and any(t in w for t in ['[-Wdiscarded-qualifiers]','[-Wstringop-overread]']),w
            if warnings:print('Vendored SQLite diagnostics:',len(warnings),flush=True)
            print(run(['ctest','--test-dir',str(b),'-R','^(sqlite_tables_contract|index_query_contract)$','--output-on-failure']).stdout,flush=True)
            result=run([str(b/'index_probe')]);(OUT/('plans-'+backend+'.log')).write_text(result.stdout)
            print(result.stdout,flush=True)
            process_contract(b)
        exe=OUT/'index-sanitized'
        run(['c++','-std=c++17','-pthread','-Wall','-Wextra','-Wpedantic','-fsanitize=address,undefined','-fno-sanitize-recover=all',
            '-I'+str(CP),'-isystem',str(ROOT/'third_party'),str(CP/'tools/index_probe.cpp'),str(OUT/'scripted/libstudy_sqlite.a'),'-ldl','-o',str(exe)])
        print(run([str(exe)]).stdout,flush=True)
    p=ROOT/'docs/learn/lessons/111.json'
    if p.exists():
        corpora={lang:[normalized(p.read_text(),lang) for p in CP.rglob('*') if p.is_file() and
            (p.suffix in ['.cpp','.h'] if lang in ['cpp','sql'] else p.name=='CMakeLists.txt')] for lang in ['cpp','sql','cmake']}
        count=0
        for section in json.loads(p.read_text())['sections']:
            for code in section.get('codes',[]):
                if code['language'] in corpora and 'text' in code:
                    assert any(normalized(code['text'],code['language']) in s for s in corpora[code['language']]),code['label']
                    count+=1
        print('inline snippets:',count)
if __name__=='__main__':main()
