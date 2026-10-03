"""Lesson120: file ownership, saved-key verification and issuance failure boundaries."""
from pathlib import Path
import contextlib,json,subprocess,tempfile,sys,sqlite3,threading
from http.server import BaseHTTPRequestHandler,ThreadingHTTPServer
from check_learning_text_layout import run
from check_learning_tables_keys import service
from check_part_docs import normalized
ROOT=Path(__file__).resolve().parents[1]
CP=ROOT/'docs/learn/checkpoints/120-account-bootstrap'
OUT=ROOT/'out/learning-checkpoints/120-account-bootstrap-check'

def invoke(build,port,folder,expected):
    p=subprocess.run([str(build/'account_bootstrap'),str(port),str(folder)],input='quit\n',text=True,capture_output=True,timeout=8)
    assert p.returncode==expected,(p.returncode,p.stdout,p.stderr)
    if (folder/'account.json').is_file():
        try:token=json.loads((folder/'account.json').read_text())['token']
        except (ValueError,KeyError):token=''
        assert not token or token not in p.stdout+p.stderr
    return p.stdout

def process_contract(build):
    with tempfile.TemporaryDirectory(prefix='bootstrap120-') as tmp:
        folder=Path(tmp)/'프로필';db=Path(tmp)/'study.db'
        with service(build/'study_account_db',db) as port:
            first=invoke(build,port,folder,0)
            assert first.startswith('online id=') and ' bp=0' in first
            content=(folder/'account.json').read_bytes()
            assert invoke(build,port,folder,0)==first
            assert (folder/'account.json').read_bytes()==content
            with sqlite3.connect(db) as conn:assert conn.execute('SELECT count(*) FROM account_keys').fetchone()==(1,)
            # Separate process cannot use the folder while the local advisory lock is held.
            if sys.platform.startswith('linux'):
                import fcntl
                with (folder/'account.lock').open('a') as lock:
                    fcntl.flock(lock,fcntl.LOCK_EX|fcntl.LOCK_NB)
                    invoke(build,port,folder,4)
                assert invoke(build,port,folder,0)==first
            (folder/'account.json').write_text('damaged')
            invoke(build,port,folder,4)
            assert (folder/'account.json').read_text()=='damaged'
            with sqlite3.connect(db) as conn:assert conn.execute('SELECT count(*) FROM account_keys').fetchone()==(1,)
            (folder/'account.json').write_bytes(content)
        assert 'verification unavailable' in invoke(build,port,folder,3)
        assert (folder/'account.json').read_bytes()==content
    print('Real SQLite service + fresh CLI processes: one account, Unicode folder, restart, offline preservation, corruption and cooperative lock passed',flush=True)

@contextlib.contextmanager
def faulty_service():
    state={'issued':0,'verified':0,'mode':'good','token':'a'*32}
    class Handler(BaseHTTPRequestHandler):
        def log_message(self,*args):pass
        def reply(self,status,body):
            data=json.dumps(body).encode();self.send_response(status);self.send_header('Content-Type','application/json');self.send_header('Content-Length',str(len(data)));self.end_headers();self.wfile.write(data)
        def do_POST(self):
            assert self.path=='/study/v1/guest'
            self.rfile.read(int(self.headers.get('Content-Length','0')));state['issued']+=1
            self.reply(201,{'player_id':42,'token':state['token']})
        def do_GET(self):
            assert self.path=='/study/v1/me'
            assert self.headers['Authorization']=='Bearer '+state['token'];state['verified']+=1
            mode=state['mode']
            if mode=='reject':self.reply(401,{})
            elif mode=='down':self.reply(503,{})
            elif mode=='bad':self.reply(200,{'player_id':42,'rp':False,'xp':0,'bp':0})
            else:self.reply(200,{'player_id':43 if mode=='wrong_id' else 42,'rp':0,'xp':0,'bp':0})
    server=ThreadingHTTPServer(('127.0.0.1',0),Handler)
    t=threading.Thread(target=server.serve_forever,daemon=True);t.start()
    try:yield server.server_port,state
    finally:server.shutdown();server.server_close();t.join(3)

def http_contract(build):
    with tempfile.TemporaryDirectory(prefix='bootstrap120-http-') as tmp, faulty_service() as (port,state):
        folder=Path(tmp)/'profile';assert 'online id=42' in invoke(build,port,folder,0)
        content=(folder/'account.json').read_bytes()
        for mode,code in [('reject',5),('down',3),('bad',3),('wrong_id',3),('good',0)]:
            state['mode']=mode;invoke(build,port,folder,code)
            assert (folder/'account.json').read_bytes()==content
            assert state['issued']==1
        snapshot=(state['issued'],state['verified'])
        record=json.loads(content);record['origin']='http://127.0.0.1:1'
        (folder/'account.json').write_text(json.dumps(record));invoke(build,port,folder,4)
        assert snapshot==(state['issued'],state['verified'])
    print('HTTP rejection/unavailable/malformed/wrong identity: key preserved, no replacement, no secret output; wrong origin has zero HTTP calls',flush=True)

def root_contract():
    flags=['c++','-std=c++17','-pthread','-Wall','-Wextra','-Wpedantic','-DCPPHTTPLIB_OPENSSL_SUPPORT','-I'+str(ROOT),'-isystem',str(ROOT/'third_party')]
    for name in ['current_account_responses','current_http_failures','match_http']:
        exe=OUT/name
        run([*flags,str(ROOT/f'tests/learning/{name}.cpp'),str(ROOT/'meta/http_client.cpp'),'-lssl','-lcrypto','-o',str(exe)],timeout=240)
        result=run([str(exe)],timeout=30)
        assert 'FAKE-CREDENTIAL-119' not in result.stderr and '[forged log line]' not in result.stderr
        print(result.stdout,flush=True)

def main():
    OUT.mkdir(parents=True,exist_ok=True)
    old=CP.parent/'119-http-failure'
    for p in old.rglob('*'):
        if p.is_file() and p.relative_to(old).as_posix() not in {'README.md','DESIGN.md','CMakeLists.txt'}:
            assert p.read_bytes()==(CP/p.relative_to(old)).read_bytes(),p
    if '--snippets-only' not in sys.argv:
        for backend in ['SCRIPTED','SDL']:
            build=OUT/backend.lower()
            run(['cmake','-S',str(CP),'-B',str(build),'-DSTUDY_PLATFORM='+backend,'-DCMAKE_BUILD_TYPE=Release','-DCMAKE_CXX_FLAGS=-Wall -Wextra -Wpedantic'])
            targets=['bootstrap_contract','account_bootstrap','json_boundary_contract','http_retry_contract']
            if backend=='SCRIPTED':targets+=['study_account_db','shop_contract','account_contract']
            r=run(['cmake','--build',str(build),'--target',*targets,'-j2'])
            (OUT/f'build-{backend}.log').write_text(r.stdout+r.stderr)
            for line in (r.stdout+r.stderr).splitlines():
                if 'warning:' in line:assert '/third_party/sqlite3.c:' in line,line
            print(run(['ctest','--test-dir',str(build),'-R','^(bootstrap_contract|json_boundary_contract|http_retry_contract)$','--output-on-failure'],timeout=80).stdout,flush=True)
            if backend=='SCRIPTED':
                with tempfile.TemporaryDirectory(prefix='bootstrap120-contracts-') as tmp:
                    for target in ['account_contract','shop_contract']:print(run([str(build/target),str(Path(tmp)/(target+'.db'))]).stdout,flush=True)
                process_contract(build);http_contract(build)
        exe=OUT/'bootstrap-sanitized'
        run(['c++','-std=c++17','-pthread','-fsanitize=address,undefined','-fno-sanitize-recover=all','-I'+str(CP),'-isystem',str(ROOT/'third_party'),str(CP/'tests/bootstrap_contract.cpp'),str(CP/'settings/private_file.cpp'),str(CP/'meta/account_file_lock.cpp'),'-lcrypto','-o',str(exe)],timeout=240)
        print(run([str(exe)],timeout=30).stdout,flush=True)
        root_contract()
    lesson=ROOT/'docs/learn/lessons/120.json'
    if lesson.exists():
        corpora={lang:[normalized(p.read_text(),lang) for p in CP.rglob('*') if p.is_file() and (p.suffix in ['.cpp','.h'] if lang=='cpp' else p.name=='CMakeLists.txt')] for lang in ['cpp','cmake']}
        n=0
        for sec in json.loads(lesson.read_text())['sections']:
            for code in sec.get('codes',[]):
                if 'text' in code and code['language'] in corpora:
                    assert any(normalized(code['text'],code['language']) in c for c in corpora[code['language']]),code['label'];n+=1
        print('inline snippets',n,flush=True)
if __name__=='__main__':main()
