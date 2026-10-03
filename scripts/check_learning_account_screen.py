"""Lesson122: UI/service isolation, key lifetime and asynchronous frame polling."""
from pathlib import Path
import json,sys,tempfile,subprocess,threading,time,contextlib
from http.server import BaseHTTPRequestHandler,ThreadingHTTPServer
from check_learning_text_layout import run
from check_learning_tables_keys import service
from check_part_docs import normalized
ROOT=Path(__file__).resolve().parents[1]
CP=ROOT/'docs/learn/checkpoints/122-account-screen'
OUT=ROOT/'out/learning-checkpoints/122-account-screen-check'

@contextlib.contextmanager
def delayed_service():
    state={'issued':0,'verified':0}
    class Handler(BaseHTTPRequestHandler):
        def log_message(self,*a):pass
        def reply(self,status,value):
            body=json.dumps(value).encode();self.send_response(status);self.send_header('Content-Type','application/json');self.send_header('Content-Length',str(len(body)));self.end_headers();self.wfile.write(body)
        def do_POST(self):
            assert self.path=='/study/v1/guest';self.rfile.read(int(self.headers['Content-Length']))
            state['issued']+=1;time.sleep(.2)
            self.reply(201,{'player_id':7,'token':'a'*32})
        def do_GET(self):
            assert self.path=='/study/v1/me' and self.headers.get('Authorization')=='Bearer '+'a'*32
            state['verified']+=1;time.sleep(.2);self.reply(200,{'player_id':7,'rp':0,'xp':0,'bp':0})
    server=ThreadingHTTPServer(('127.0.0.1',0),Handler);thread=threading.Thread(target=server.serve_forever,daemon=True);thread.start()
    try:yield server.server_port,state
    finally:server.shutdown();server.server_close();thread.join(3)

def processes(build):
    with tempfile.TemporaryDirectory(prefix='account-ui122-') as tmp:
        with delayed_service() as (port,state):
            folder=Path(tmp)/'delayed'
            text=run([str(build/'account_ui_probe'),str(port),str(folder)],timeout=15).stdout
            assert 'status=2 unsaved=0 id=7' in text,text
            assert int(text.split('frames=')[1].split()[0])>1,text
            assert state=={'issued':1,'verified':1},state
            text=run([str(build/'account_ui_probe'),str(port),str(folder)],timeout=15).stdout
            assert 'id=7' in text and state=={'issued':1,'verified':2}
            for path in folder.glob('*'):
                if path.name.endswith('.json'):assert path.stat().st_mode&0o777==0o600
        with service(build/'study_account_db',Path(tmp)/'server.db') as port:
            folder=Path(tmp)/'real'
            a=run([str(build/'account_ui_probe'),str(port),str(folder)],timeout=15).stdout
            b=run([str(build/'account_ui_probe'),str(port),str(folder)],timeout=15).stdout
            assert 'status=2 unsaved=0 id=' in a and a.split(' id=')[1]==b.split(' id=')[1],(a,b)
            held=subprocess.Popen([str(build/'account_ui_probe'),str(port),str(folder),'--hold'],stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True)
            try:
                import select
                assert select.select([held.stdout],[],[],10)[0], 'held probe result timeout'
                assert 'status=2 unsaved=0' in held.stdout.readline()
                blocked=subprocess.run([str(build/'account_ui_probe'),str(port),str(folder)],capture_output=True,text=True,timeout=10)
                assert blocked.returncode==5 and 'status=5 unsaved=0' in blocked.stdout,blocked.stdout
                held.communicate('done\n',timeout=10);assert held.returncode==0
            finally:
                if held.poll() is None:held.kill();held.communicate(timeout=3)

        print('Real service: asynchronous account issuance/persistence/restart; delayed reply permits progress with one request',flush=True)

def main():
    OUT.mkdir(parents=True,exist_ok=True)
    old=CP.parent/'121-save-uncertainty'
    changed={'README.md','DESIGN.md','CMakeLists.txt','meta/account_bootstrap.h','src/main.cpp','client/menu_model.h','renderer/menu_labels.h','renderer/menu_controls.h','tests/widget_state_contract.cpp'}
    for p in old.rglob('*'):
        if p.is_file() and p.relative_to(old).as_posix() not in changed:assert p.read_bytes()==(CP/p.relative_to(old)).read_bytes(),p
    if '--snippets-only' not in sys.argv:
        for backend in ['SCRIPTED','SDL']:
            build=OUT/backend.lower()
            run(['cmake','-S',str(CP),'-B',str(build),'-DSTUDY_PLATFORM='+backend,'-DCMAKE_BUILD_TYPE=Release','-DCMAKE_CXX_FLAGS=-Wall -Wextra -Wpedantic'])
            targets=['account_ui_contract','account_ui_probe','bootstrap_contract','outbox_contract','widget_state_contract','settings_contract']
            targets+=['study_account_db'] if backend=='SCRIPTED' else ['tetris']
            r=run(['cmake','--build',str(build),'--target',*targets,'-j2']);(OUT/f'build-{backend}.log').write_text(r.stdout+r.stderr)
            print(backend,'targets built',flush=True)
            print(run(['ctest','--test-dir',str(build),'-R','^(account_ui_contract|bootstrap_contract|outbox_contract|widget_state_contract|settings_contract)$','--output-on-failure'],timeout=60).stdout,flush=True)
            if backend=='SCRIPTED':processes(build)
            if backend=='SDL':
                assert '--account PORT FOLDER' in run([str(build/'tetris'),'--help']).stdout
                p=subprocess.run([str(build/'tetris'),'--account','65536','unused'],capture_output=True,timeout=5);assert p.returncode==2
        exe=OUT/'account-ui-sanitized'
        run(['c++','-std=c++17','-pthread','-fsanitize=address,undefined','-fno-sanitize-recover=all','-I'+str(CP),'-isystem',str(ROOT/'third_party'),str(CP/'tests/account_ui_contract.cpp'),'-o',str(exe)],timeout=240)
        print(run([str(exe)],timeout=30).stdout,flush=True)
    lesson=ROOT/'docs/learn/lessons/122.json'
    if lesson.exists():
        corpora={lang:[normalized(p.read_text(),lang) for p in CP.rglob('*') if p.is_file() and (p.suffix in ['.cpp','.h'] if lang=='cpp' else p.name=='CMakeLists.txt')] for lang in ['cpp','cmake']}
        n=0
        for sec in json.loads(lesson.read_text())['sections']:
            for code in sec.get('codes',[]):
                if 'text' in code and code['language'] in corpora:
                    assert any(normalized(code['text'],code['language']) in c for c in corpora[code['language']]),code['label'];n+=1
        print('inline snippets',n,flush=True)
if __name__=='__main__':main()
