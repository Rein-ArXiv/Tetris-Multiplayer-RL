"""Check persistent lesson110 storage, relational constraints and unchanged caller."""
from pathlib import Path
import contextlib,json,subprocess,sys,tempfile,time,sqlite3,re
from concurrent.futures import ThreadPoolExecutor
from check_learning_text_layout import run
from check_learning_meta_service import request
from check_part_docs import normalized
ROOT=Path(__file__).resolve().parents[1]
CP=ROOT/'docs/learn/checkpoints/110-tables-keys'
OUT=ROOT/'out/learning-checkpoints/110-tables-keys-check'
@contextlib.contextmanager
def service(executable,db):
    log=db.with_suffix('.log')
    with log.open('w') as output:proc=subprocess.Popen([str(executable),'0',str(db)],stdout=output,stderr=output)
    try:
        deadline=time.monotonic()+8;port=None
        while time.monotonic()<deadline:
            assert proc.poll() is None,log.read_text()
            for line in log.read_text().splitlines():
                if line.startswith('PORT '):port=int(line.split()[1])
            if port:
                try:
                    if request(port,path='/healthz')==(200,{'ok':True}):break
                except OSError:pass
            time.sleep(.02)
        else:raise AssertionError('readiness timeout')
        yield port
    finally:
        if proc.poll() is None:proc.terminate()
        try:proc.wait(timeout=3)
        except subprocess.TimeoutExpired:proc.kill();proc.wait(timeout=3)

def process_contract(build):
    server=build/'study_meta_db';client=build/'meta_submit'
    with tempfile.TemporaryDirectory(prefix='study-meta-110-') as tmp:
        db=Path(tmp)/'study.db'
        with service(server,db) as port:
            first=run([str(client),str(port),'17']).stdout
            assert 'confirmed key=17 row=1 ' in first,first
            assert run([str(client),str(port),'17']).stdout==first
            record=dict(key=18,round=1,player_a=101,player_b=202,ticks=11,score_a=100,score_b=0,lines_a=0,lines_b=0,winner=0)
            with ThreadPoolExecutor(max_workers=4) as workers:
                replies=list(workers.map(lambda _:request(port,record),range(12)))
            assert all(r==replies[0] for r in replies) and replies[0][0]==200 and replies[0][1]['row']==2
            assert request(port,dict(record,score_a=99))[0]==409
            assert request(port,dict(record,key=19,player_b=999))[0]==400
            assert request(port,dict(record,key=19,winner=None))[0]==400 # study wire uses integer0 for draw
            wide=dict(record,key=2**64-1,round=2**64-1,ticks=2**64-1,score_a=2**64-1,lines_a=2**32-1)
            assert request(port,wide)[0]==200
            assert request(port,raw=b' '*1025)[0]==413
        with sqlite3.connect(db) as conn:
            assert conn.execute('SELECT count(*) FROM matches').fetchone()==(3,)
            assert conn.execute("SELECT winner FROM matches WHERE match_key='18'").fetchone()==(None,)
            assert conn.execute("SELECT typeof(score_a),score_a FROM matches WHERE match_key=?",(str(2**64-1),)).fetchone()==('text',str(2**64-1))
        with service(server,db) as port:
            assert run([str(client),str(port),'17']).stdout==first
            assert request(port,record)==replies[0]
            # An independent writer holds the file lock; never report a receipt for failed write.
            with sqlite3.connect(db) as writer:
                writer.execute('BEGIN IMMEDIATE')
                assert request(port,dict(record,key=20))[0]==503
                writer.rollback()
            assert request(port,dict(record,key=20))[0]==200
        bad=subprocess.run([str(server),'0',str(Path(tmp)/'missing'/'study.db')],text=True,capture_output=True,timeout=4)
        assert bad.returncode==1 and 'PORT ' not in bad.stdout and 'database service:' in bad.stderr
    print('HTTP persistence, restart, duplicate/conflict, concurrency, uint64, NULL and storage failure: passed',flush=True)

def main():
    OUT.mkdir(parents=True,exist_ok=True)
    for p in (CP.parent/'109-meta-service').rglob('*'):
        rel=p.relative_to(CP.parent/'109-meta-service')
        if p.is_file() and str(rel) not in {'README.md','DESIGN.md','CMakeLists.txt'}:assert p.read_bytes()==(CP/rel).read_bytes(),rel
    if '--snippets-only' not in sys.argv:
        for backend in ['SCRIPTED','SDL']:
            build=OUT/backend.lower()
            run(['cmake','-S',str(CP),'-B',str(build),'-DSTUDY_PLATFORM='+backend,'-DCMAKE_BUILD_TYPE=Release','-DCMAKE_CXX_FLAGS=-Wall -Wextra -Wpedantic'])
            result=run(['cmake','--build',str(build),'--target','study_meta_db','sqlite_tables_contract','meta_submit','-j2'])
            (OUT/('build-'+backend+'.log')).write_text(result.stdout+result.stderr)
            warnings=[line for line in (result.stdout+result.stderr).splitlines() if 'warning:' in line]
            for line in warnings:
                assert '/third_party/sqlite3.c:' in line and any(tag in line for tag in
                    ['[-Wdiscarded-qualifiers]','[-Wstringop-overread]']),line
            if warnings:print('Vendored SQLite compiler diagnostics (recorded separately):',len(warnings),flush=True)
            print(run(['ctest','--test-dir',str(build),'-R','^sqlite_tables_contract$','--output-on-failure']).stdout,flush=True)
            process_contract(build)
        exe=OUT/'contract-sanitized'
        # SQLite is the same C11 Release archive; sanitizers instrument the new C++ ownership/adapter code.
        run(['c++','-std=c++17','-pthread','-Wall','-Wextra','-Wpedantic','-fsanitize=address,undefined','-fno-sanitize-recover=all',
             '-I'+str(CP),'-isystem',str(ROOT/'third_party'),str(CP/'tests/sqlite_tables_contract.cpp'),str(OUT/'scripted/libstudy_sqlite.a'),'-ldl','-o',str(exe)])
        with tempfile.TemporaryDirectory(prefix='tables-sanitizer-') as tmp:print(run([str(exe),str(Path(tmp)/'test.db')]).stdout,flush=True)
    lesson=ROOT/'docs/learn/lessons/110.json'
    if lesson.exists():
        corpora={lang:[normalized(p.read_text(),lang) for p in CP.rglob('*') if p.is_file() and
            (p.suffix in ['.h','.cpp'] if lang in ['cpp','sql'] else p.name=='CMakeLists.txt')] for lang in ['cpp','sql','cmake']}
        count=0
        for section in json.loads(lesson.read_text())['sections']:
            for code in section.get('codes',[]):
                if code['language'] in corpora and 'text' in code:
                    assert any(normalized(code['text'],code['language']) in t for t in corpora[code['language']]),code['label']
                    count+=1
        print('inline snippets:',count)
if __name__=='__main__':main()
