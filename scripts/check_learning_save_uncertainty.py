"""Lesson121: durable request barrier, restart replay and truthful result projection."""
from pathlib import Path
import contextlib,json,subprocess,tempfile,threading,sys,sqlite3,http.client
from http.server import BaseHTTPRequestHandler,ThreadingHTTPServer
from check_learning_text_layout import run
from check_learning_tables_keys import service
from check_part_docs import normalized
from check_learning_result_rating import check as rating_check
ROOT=Path(__file__).resolve().parents[1]
CP=ROOT/'docs/learn/checkpoints/121-save-uncertainty'
OUT=ROOT/'out/learning-checkpoints/121-save-uncertainty-check'

def cli(build,port,folder,command,expected,key=None):
    args=[str(build/'meta_outbox'),str(port),str(folder),command]
    if key is not None:args.append(str(key))
    p=subprocess.run(args,input='quit\n',text=True,capture_output=True,timeout=12)
    assert p.returncode==expected,(p.returncode,p.stdout,p.stderr)
    return p.stdout

@contextlib.contextmanager
def proxy(backend):
    state={'mode':'drop','bodies':[]}
    class Handler(BaseHTTPRequestHandler):
        def log_message(self,*args):pass
        def do_POST(self):
            assert self.path=='/study/v1/matches'
            body=self.rfile.read(int(self.headers['Content-Length']));state['bodies'].append(body)
            if state['mode']=='stop':status,data=409,b'{}'
            elif state['mode']=='bad':
                request=json.loads(body);status=200
                data=json.dumps(dict(key=request['key']+1,row=1,player_a=request['player_a'],player_b=request['player_b'])).encode()
            else:
                conn=http.client.HTTPConnection('127.0.0.1',backend,timeout=5)
                try:
                    conn.request('POST',self.path,body,{'Content-Type':'application/json'})
                    reply=conn.getresponse();status,data=reply.status,reply.read()
                finally:conn.close()
                if state['mode']=='drop':
                    self.close_connection=True;return # committed upstream; deliberately lose every reply
            self.send_response(status);self.send_header('Content-Type','application/json');self.send_header('Content-Length',str(len(data)));self.end_headers();self.wfile.write(data)
    server=ThreadingHTTPServer(('127.0.0.1',0),Handler)
    worker=threading.Thread(target=server.serve_forever,daemon=True);worker.start()
    try:yield server.server_port,state
    finally:server.shutdown();server.server_close();worker.join(3)

def process_contract(build):
    with tempfile.TemporaryDirectory(prefix='outbox121-') as tmp:
        db=Path(tmp)/'study.db';folder=Path(tmp)/'보관함'
        with service(build/'study_meta_db',db) as backend,proxy(backend) as (port,state):
            with sqlite3.connect(db) as conn:
                initial_balances=conn.execute('SELECT w.player_id,w.bp,c.xp,c.rp FROM wallets w JOIN careers c USING(player_id) ORDER BY w.player_id').fetchall()
            cli(build,port,folder,'prepare',0,121)
            assert state['bodies']==[]
            initial=(folder/'outbox.json').read_bytes()
            assert 'unconfirmed' in cli(build,port,folder,'resume',3)
            assert len(state['bodies'])==3 and len(set(state['bodies']))==1
            assert (folder/'outbox.json').read_bytes()==initial
            with sqlite3.connect(db) as conn:
                assert conn.execute('SELECT count(*) FROM matches').fetchone()==(1,)
                balances=conn.execute('SELECT w.player_id,w.bp,c.xp,c.rp FROM wallets w JOIN careers c USING(player_id) ORDER BY w.player_id').fetchall()
            assert len(balances)==2 and all(row[1:]==(0,0,0) for row in initial_balances)
            assert sorted(row[1] for row in balances)==[3,10],balances
            state['mode']='good'
            resumed=cli(build,port,folder,'resume',0)
            assert 'server confirmed key=121 row=1' in resumed
            assert len(state['bodies'])==4 and len(set(state['bodies']))==1
            with sqlite3.connect(db) as conn:
                assert conn.execute('SELECT count(*) FROM matches').fetchone()==(1,)
                assert conn.execute('SELECT w.player_id,w.bp,c.xp,c.rp FROM wallets w JOIN careers c USING(player_id) ORDER BY w.player_id').fetchall()==balances
            done=json.loads((folder/'outbox.json').read_text())
            assert done['status']=='confirmed' and json.loads(done['receipt'])['row']==1
            assert cli(build,port,folder,'resume',0)==resumed and len(state['bodies'])==4
            cli(build,port,folder,'prepare',5,999) # a different match cannot overwrite this receipt
            assert json.loads((folder/'outbox.json').read_text())==done
            other=Path(tmp)/'stopped';cli(build,port,other,'prepare',0,122);state['mode']='stop'
            cli(build,port,other,'resume',4);count=len(state['bodies']);state['mode']='good'
            cli(build,port,other,'resume',4);assert len(state['bodies'])==count
            bad=Path(tmp)/'bad-receipt';cli(build,port,bad,'prepare',0,123);state['mode']='bad'
            cli(build,port,bad,'resume',4);assert json.loads((bad/'outbox.json').read_text())['status']=='stopped'
            count=len(state['bodies'])
            record=json.loads((folder/'outbox.json').read_text());record['origin']='http://127.0.0.1:1'
            (folder/'outbox.json').write_text(json.dumps(record));cli(build,port,folder,'resume',5)
            assert len(state['bodies'])==count
            (folder/'outbox.json').write_text('damaged');cli(build,port,folder,'resume',5)
            assert (folder/'outbox.json').read_text()=='damaged' and len(state['bodies'])==count
            if sys.platform.startswith('linux'):
                import fcntl
                with (other/'outbox.lock').open('a') as lock:
                    fcntl.flock(lock,fcntl.LOCK_EX|fcntl.LOCK_NB);cli(build,port,other,'resume',5)
            assert len(state['bodies'])==count
    print('Real upstream commit + lost replies + process restart: same body, one DB row/reward effect, stored receipt skips HTTP; stopped/corrupt/origin/lock guards passed',flush=True)

def main():
    OUT.mkdir(parents=True,exist_ok=True)
    old=CP.parent/'120-account-bootstrap'
    changed={'README.md','DESIGN.md','CMakeLists.txt','meta/local_account_store.h','tools/meta_submit.cpp'}
    for p in old.rglob('*'):
        if p.is_file() and p.relative_to(old).as_posix() not in changed:
            assert p.read_bytes()==(CP/p.relative_to(old)).read_bytes(),p
    if '--snippets-only' not in sys.argv:
        for backend in ['SCRIPTED','SDL']:
            build=OUT/backend.lower()
            run(['cmake','-S',str(CP),'-B',str(build),'-DSTUDY_PLATFORM='+backend,'-DCMAKE_BUILD_TYPE=Release','-DCMAKE_CXX_FLAGS=-Wall -Wextra -Wpedantic'])
            targets=['outbox_contract','bootstrap_contract','meta_outbox','meta_submit']
            if backend=='SCRIPTED':targets+=['study_meta_db','http_retry_contract']
            result=run(['cmake','--build',str(build),'--target',*targets,'-j2'])
            (OUT/f'build-{backend}.log').write_text(result.stdout+result.stderr)
            for line in (result.stdout+result.stderr).splitlines():
                if 'warning:' in line:assert '/third_party/sqlite3.c:' in line,line
            names='outbox_contract|bootstrap_contract'+('|http_retry_contract' if backend=='SCRIPTED' else '')
            print(run(['ctest','--test-dir',str(build),'-R','^('+names+')$','--output-on-failure'],timeout=80).stdout,flush=True)
            if backend=='SCRIPTED':
                process_contract(build)
                with tempfile.TemporaryDirectory(prefix='submit121-') as tmp:
                    with service(build/'study_meta_db',Path(tmp)/'study.db') as port:
                        a=run([str(build/'meta_submit'),str(port),'88']).stdout
                        b=run([str(build/'meta_submit'),str(port),'88']).stdout
                        assert a==b and a.startswith('confirmed key=88 row=1')
                print('Shared trusted simulation fixture preserves the existing meta_submit output',flush=True)
        exe=OUT/'outbox-sanitized'
        run(['c++','-std=c++17','-pthread','-fsanitize=address,undefined','-fno-sanitize-recover=all','-I'+str(CP),'-isystem',str(ROOT/'third_party'),str(CP/'tests/outbox_contract.cpp'),str(CP/'settings/private_file.cpp'),str(CP/'meta/account_file_lock.cpp'),'-o',str(exe)],timeout=240)
        print(run([str(exe)],timeout=30).stdout,flush=True)
        rating_check()
    lesson=ROOT/'docs/learn/lessons/121.json'
    if lesson.exists():
        corpora={lang:[normalized(p.read_text(),lang) for p in CP.rglob('*') if p.is_file() and (p.suffix in ['.cpp','.h'] if lang=='cpp' else p.name=='CMakeLists.txt')] for lang in ['cpp','cmake']}
        n=0
        for sec in json.loads(lesson.read_text())['sections']:
            for code in sec.get('codes',[]):
                if 'text' in code and code['language'] in corpora:
                    assert any(normalized(code['text'],code['language']) in c for c in corpora[code['language']]),code['label'];n+=1
        print('inline snippets',n,flush=True)
if __name__=='__main__':main()
