"""Lesson142: durable exact-request retries with real files, HTTP and process restarts."""
from pathlib import Path
import argparse,contextlib,json,os,socket,sqlite3,subprocess,tempfile,threading,urllib.request,urllib.error
from http.server import BaseHTTPRequestHandler,ThreadingHTTPServer
from check_learning_text_layout import run
from check_learning_account_change import server,request
from check_part_docs import normalized
R=Path(__file__).resolve().parents[1];CP=R/'docs/learn/checkpoints/142-recovery-journal';OUT=R/'out/learning-checkpoints/142-recovery-journal-check'
@contextlib.contextmanager
def proxy(base):
    state={'mode':'pass','bodies':[],'changes':0,'gets':0}
    class Handler(BaseHTTPRequestHandler):
        def log_message(self,*args):pass
        def handle_request(self):
            raw=self.rfile.read(int(self.headers.get('Content-Length','0'))) if self.command=='POST' else None
            is_change='/account/' in self.path
            if is_change:
                state['changes']+=1;state['bodies'].append(raw)
                if state['mode'] in {'503','429'}:
                    self.send_reply(int(state['mode']),{});return
            token=self.headers.get('Authorization','').removeprefix('Bearer ') or None
            status,body,_=request(base,self.path,token=token,raw=raw)
            if is_change:
                if state['mode']=='drop':
                    self.connection.shutdown(socket.SHUT_RDWR);self.connection.close();return
                if state['mode']=='wrong_id':body={'player_id':body.get('player_id',1)+1,'auth_epoch':1}
                if state['mode']=='malformed':body={'player_id':True,'auth_epoch':0}
            self.send_reply(status,body)
        def send_reply(self,status,body):
            data=json.dumps(body).encode();self.send_response(status);self.send_header('Content-Type','application/json');self.send_header('Content-Length',str(len(data)));self.end_headers();self.wfile.write(data)
        do_POST=handle_request;do_GET=handle_request
    srv=ThreadingHTTPServer(('127.0.0.1',0),Handler);t=threading.Thread(target=srv.serve_forever,daemon=True);t.start()
    try:yield srv.server_port,state
    finally:srv.shutdown();srv.server_close();t.join(3)
def invoke(b,port,folder,action,expected,fault=None):
    cmd=[str(b/'account_journal'),str(port),str(folder),action]+([fault] if fault else [])
    p=subprocess.run(cmd,text=True,capture_output=True,timeout=12)
    assert p.returncode==expected,(action,p.returncode,p.stdout,p.stderr)
    for name,keys in [('account.json',['token']),('recovery.json',['recovery_code']),('change.pending.json',['credential','next_token','next_recovery'])]:
        path=folder/name
        if path.is_file():
            try:values=json.loads(path.read_text())
            except ValueError:continue
            for key in keys:
                if key in values:assert values[key] not in p.stdout+p.stderr,'secret output'
    return p.stdout

def integration(b):
    with tempfile.TemporaryDirectory(prefix='study142-')as tmp:
        root=Path(tmp);db=root/'server.db'
        with server(b/'study_account_db',db)as base,proxy(base)as(port,state):
            origin=f'http://127.0.0.1:{port}'
            def epoch(id):
                with sqlite3.connect(db)as con:return con.execute('SELECT auth_epoch FROM account_keys WHERE player_id=?',(str(id),)).fetchone()[0]
            for i,mode in enumerate(['pass','drop','wrong_id','malformed','503','429','crash-pending','crash-recovery','crash-account','fail-account','fail-clear']):
                folder=root/('한글-'+str(i));state['mode']='pass';invoke(b,port,folder,'connect',0)
                old=json.loads((folder/'account.json').read_text());id=old['player_id'];before=state['changes'];state['bodies']=[]
                state['mode']=mode if mode in ['drop','wrong_id','malformed','503','429']else'pass'
                fault=mode if mode.startswith(('crash-','fail-'))else None
                expected=70 if mode.startswith('crash-')else 10 if mode.startswith('fail-')else 0 if mode=='pass'else 8
                invoke(b,port,folder,'rotate',expected,fault)
                if mode=='pass':assert epoch(id)==1;continue
                journal=json.loads((folder/'change.pending.json').read_text());assert journal['operation']=='rotate'
                assert epoch(id)==(0 if mode in ['503','429','crash-pending']else 1)
                if mode=='crash-pending':assert state['changes']==before
                if mode in ['fail-account','crash-recovery']:assert json.loads((folder/'account.json').read_text())==old
                # Even the old bootstrap must refuse to issue while recovery is pending.
                p=subprocess.run([str(b/'account_bootstrap'),str(port),str(folder)],capture_output=True,timeout=10);assert p.returncode==4
                state['mode']='pass';invoke(b,port,folder,'connect',0)
                assert epoch(id)==1
                active=json.loads((folder/'account.json').read_text());backup=json.loads((folder/'recovery.json').read_text())
                assert active['player_id']==backup['player_id']==id and active['token']==journal['next_token']
                assert backup['recovery_code']==journal['next_recovery']
                assert json.loads((folder/'change.pending.json').read_text())=={'origin':origin,'state':'idle'}
                if len(state['bodies'])>1:assert len(set(state['bodies']))==1
                if os.name!='nt':
                    for name in ['account.json','recovery.json','change.pending.json']:assert (folder/name).stat().st_mode&0o777==0o600
                invoke(b,port,folder,'connect',0);assert epoch(id)==1
            for mode in ['uncertain-pending','uncertain-account','uncertain-clear']:
                folder=root/mode;invoke(b,port,folder,'connect',0);old=json.loads((folder/'account.json').read_text());before=state['changes']
                invoke(b,port,folder,'rotate',4 if mode=='uncertain-pending' else 10,mode)
                assert epoch(old['player_id'])==(0 if mode=='uncertain-pending' else 1)
                if mode=='uncertain-pending':assert state['changes']==before
                invoke(b,port,folder,'connect',0)
                assert epoch(old['player_id'])==1
                assert json.loads((folder/'change.pending.json').read_text())['state']=='idle'
            folder=root/'damaged';invoke(b,port,folder,'connect',0);account=json.loads((folder/'account.json').read_text());before=state['changes']
            pending=folder/'change.pending.json'
            for body in ['broken',json.dumps({'origin':'http://foreign','state':'idle'}),json.dumps({'origin':origin,'operation':'rotate','credential':account['token'],'next_token':account['token'],'next_recovery':'rc1.'+'c'*64,'expected_player_id':account['player_id']})]:
                pending.write_text(body);invoke(b,port,folder,'resume',4);assert pending.read_text()==body and state['changes']==before
            pending.unlink();pending.mkdir();invoke(b,port,folder,'rotate',4);assert state['changes']==before;pending.rmdir()
            if os.name!='nt':
                import fcntl
                with (folder/'account.lock').open('a')as lock:
                    fcntl.flock(lock,fcntl.LOCK_EX|fcntl.LOCK_NB);invoke(b,port,folder,'rotate',4);assert state['changes']==before
                invoke(b,port,folder,'backup',0)
            # Restore intentionally uses backup identity when the access file is lost.
            (folder/'account.json').unlink();invoke(b,port,folder,'recover',0)
            assert json.loads((folder/'account.json').read_text())['player_id']==account['player_id']
            # A genuine rejection clears this attempt; no automatic new account follows.
            folder=root/'rejected';invoke(b,port,folder,'connect',0);old=json.loads((folder/'account.json').read_text());state['mode']='503';invoke(b,port,folder,'rotate',8)
            request(base,'/study/v1/account/rotate',{'credential':old['token'],'next_token':'a'*32,'next_recovery':'rc1.'+'b'*64})
            state['mode']='pass';invoke(b,port,folder,'resume',9);assert json.loads((folder/'change.pending.json').read_text())['state']=='idle'
            assert json.loads((folder/'account.json').read_text())==old
    print('Real HTTP/file/process: loss, malformed/wrong ID, throttling, crash boundaries, partial save, exact retry, permissions, lock, corruption, rejection and restoration passed',flush=True)
def main():
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('--boost',default='/usr/include');ap.add_argument('--snippets-only',action='store_true');a=ap.parse_args()
    for p in (CP.parent/'141-account-change').rglob('*'):
        if p.is_file()and p.relative_to(CP.parent/'141-account-change').as_posix()not in {'README.md','CMakeLists.txt','meta/local_account_store.h'}:assert p.read_bytes()==(CP/p.relative_to(CP.parent/'141-account-change')).read_bytes(),p
    OUT.mkdir(parents=True,exist_ok=True);b=OUT/'scripted'
    if not a.snippets_only:
        run(['cmake','-S',str(CP),'-B',str(b),'-DSTUDY_PLATFORM=SCRIPTED','-DSTUDY_AUDIO=NONE','-DCMAKE_BUILD_TYPE=Release','-DSTUDY_BOOST_INCLUDE='+str(Path(a.boost).resolve())])
        result=run(['cmake','--build',str(b),'--target','account_journal','journal_contract','study_account_db','account_bootstrap','bootstrap_contract','-j2'],timeout=300);(OUT/'build.log').write_text(result.stdout+result.stderr)
        print(run(['ctest','--test-dir',str(b),'-R','^(journal_contract|bootstrap_contract)$','--output-on-failure']).stdout,flush=True)
        integration(b)
    lesson=R/'docs/learn/lessons/142.json'
    if lesson.exists():
        corpus=[normalized(p.read_text(),'cpp')for p in CP.rglob('*')if p.suffix in {'.cpp','.h'}];count=0
        for s in json.loads(lesson.read_text())['sections']:
            ref=s.get('reference')
            if ref:assert ref['symbol']in(R/ref['path']).read_text(),ref
            for code in s.get('codes',[]):
                if 'text'in code and code['language']=='cpp':assert any(normalized(code['text'],'cpp')in c for c in corpus),code['label'];count+=1
        print('Inline snippets',count,'current symbols and cumulative preservation passed',flush=True)
if __name__=='__main__':main()
