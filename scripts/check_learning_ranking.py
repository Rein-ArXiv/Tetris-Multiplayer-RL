"""Lesson123: committed ranking projection, numeric identifiers and UI result states."""
from pathlib import Path
import json,sys,tempfile,subprocess,threading,contextlib,sqlite3
from http.server import BaseHTTPRequestHandler,ThreadingHTTPServer
from check_learning_text_layout import run
from check_learning_tables_keys import service
from check_learning_meta_service import request
from check_learning_account_screen import processes as account_processes
from check_part_docs import normalized
ROOT=Path(__file__).resolve().parents[1];CP=ROOT/'docs/learn/checkpoints/123-ranking';OUT=ROOT/'out/learning-checkpoints/123-ranking-check'

@contextlib.contextmanager
def reply_server(body,status=200):
    class Handler(BaseHTTPRequestHandler):
        def log_message(self,*a):pass
        def do_GET(self):
            self.send_response(status);self.send_header('Content-Length',str(len(body)));self.end_headers();self.wfile.write(body)
    server=ThreadingHTTPServer(('127.0.0.1',0),Handler);t=threading.Thread(target=server.serve_forever,daemon=True);t.start()
    try:yield server.server_port
    finally:server.shutdown();server.server_close();t.join(3)

def processes(build):
    with tempfile.TemporaryDirectory(prefix='ranking123-') as tmp:
        db=Path(tmp)/'matches.db'
        with service(build/'study_meta_db',db) as port:
            initial=request(port,path='/study/v1/ranking');assert initial==(200,[{'player_id':101,'rp':0},{'player_id':202,'rp':0}]),initial
            a=run([str(build/'meta_submit'),str(port),'123']).stdout
            first=request(port,path='/study/v1/ranking');assert first[0]==200
            with sqlite3.connect(db) as c:expected=sorted([(int(x[0]),x[1])for x in c.execute('SELECT player_id,rp FROM careers')],key=lambda x:(-x[1],x[0]))[:3]
            assert first[1]==[dict(player_id=i,rp=rp)for i,rp in expected]
            assert any(row['rp']>0 for row in first[1])
            assert run([str(build/'meta_submit'),str(port),'123']).stdout==a
            assert request(port,path='/study/v1/ranking')==first
            assert json.loads(run([str(build/'ranking_probe'),str(port)]).stdout)==first[1]
            assert request(port,path='/study/v1/ranking?limit=999')[0]==400
            with sqlite3.connect(db) as c:
                c.execute('PRAGMA ignore_check_constraints=ON');c.execute("UPDATE careers SET rp=-1 WHERE player_id=?",(str(first[1][-1]['player_id']),))
            assert request(port,path='/study/v1/ranking')==(503,{'error':'ranking_unavailable'})
        with service(build/'study_account_db',Path(tmp)/'accounts.db') as port:
            assert request(port,path='/study/v1/ranking')==(200,[])
            status,guest=request(port,body={},path='/study/v1/guest')
            assert status==201
            assert request(port,path='/study/v1/ranking')==(200,[{'player_id':guest['player_id'],'rp':0}])
        # Two service processes observe the same committed SQLite file.
        shared=Path(tmp)/'shared.db'
        with service(build/'study_account_db',shared) as public,service(build/'study_meta_db',shared) as writer:
            before=request(public,path='/study/v1/ranking');assert before[0]==200 and len(before[1])==2
            run([str(build/'meta_submit'),str(writer),'124'])
            after=request(public,path='/study/v1/ranking');assert after[0]==200 and after!=before
            assert after==request(writer,path='/study/v1/ranking')
        for body,status in [(b'[]',200),(b'[]',503),(b'[{"player_id":1,"rp":0,"rp":2}]',200),(b' '*2048,200),(b'[1]',200)]:
            with reply_server(body,status) as port:
                r=subprocess.run([str(build/'ranking_probe'),str(port)],capture_output=True,text=True,timeout=8)
                assert r.returncode==(0 if body==b'[]' and status==200 else 4),(r.returncode,r.stdout)
    account_processes(build)
    print('Ranking HTTP: saved results, replay, successful empty, corrupt later row, bounded receive and public fields passed',flush=True)

def main():
    OUT.mkdir(parents=True,exist_ok=True)
    old=CP.parent/'122-account-screen'
    changed={'README.md','DESIGN.md','CMakeLists.txt','meta/account_task.h','client/account_controller.h','src/main.cpp','tools/account_ui_probe.cpp','tests/account_ui_contract.cpp','meta/sqlite_results.h','meta/account_service.cpp','meta/database_service.cpp','client/menu_model.h','renderer/menu_labels.h','renderer/menu_controls.h','tests/widget_state_contract.cpp'}
    for p in old.rglob('*'):
        if p.is_file() and p.relative_to(old).as_posix() not in changed:assert p.read_bytes()==(CP/p.relative_to(old)).read_bytes(),p
    if '--snippets-only' not in sys.argv:
        for backend in ['SCRIPTED','SDL']:
            build=OUT/backend.lower()
            run(['cmake','-S',str(CP),'-B',str(build),'-DSTUDY_PLATFORM='+backend,'-DCMAKE_BUILD_TYPE=Release','-DCMAKE_CXX_FLAGS=-Wall -Wextra -Wpedantic'])
            targets=['ranking_contract','ranking_probe','account_ui_contract','bootstrap_contract','widget_state_contract']
            if backend=='SCRIPTED':targets+=['study_meta_db','study_account_db','meta_submit','account_ui_probe']
            else:targets+=['tetris']
            r=run(['cmake','--build',str(build),'--target',*targets,'-j2']);(OUT/f'build-{backend}.log').write_text(r.stdout+r.stderr);print(backend,'targets built',flush=True)
            print(run(['ctest','--test-dir',str(build),'-R','^(ranking_contract|account_ui_contract|bootstrap_contract|widget_state_contract)$','--output-on-failure'],timeout=60).stdout,flush=True)
            if backend=='SCRIPTED':processes(build)
        exe=OUT/'ranking-sanitized'
        run(['c++','-std=c++17','-pthread','-fsanitize=address,undefined','-fno-sanitize-recover=all','-I'+str(CP),'-isystem',str(ROOT/'third_party'),str(CP/'tests/ranking_contract.cpp'),str(OUT/'scripted/libstudy_sqlite.a'),'-ldl','-lcrypto','-o',str(exe)],timeout=240)
        print(run([str(exe)],timeout=30).stdout,flush=True)
    lesson=ROOT/'docs/learn/lessons/123.json'
    if lesson.exists():
        corpora={lang:[normalized(p.read_text(),lang)for p in CP.rglob('*')if p.is_file()and(p.suffix in ['.cpp','.h']if lang=='cpp'else p.name=='CMakeLists.txt')]for lang in ['cpp','cmake']}
        n=0
        for sec in json.loads(lesson.read_text())['sections']:
            for code in sec.get('codes',[]):
                if 'text'in code and code['language']in corpora:
                    assert any(normalized(code['text'],code['language'])in c for c in corpora[code['language']]),code['label'];n+=1
        print('inline snippets',n,flush=True)
if __name__=='__main__':main()
