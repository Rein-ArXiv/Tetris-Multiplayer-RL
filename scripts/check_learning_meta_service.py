"""Check lesson109's independent HTTP service process and caller lifetime."""
from pathlib import Path
import contextlib,json,subprocess,sys,tempfile,time,urllib.request,urllib.error
from concurrent.futures import ThreadPoolExecutor
from check_learning_text_layout import run
from check_part_docs import normalized
ROOT=Path(__file__).resolve().parents[1]
CP=ROOT/'docs/learn/checkpoints/109-meta-service'
OUT=ROOT/'out/learning-checkpoints/109-meta-service-check'

def request(port,body=None,raw=None,path='/study/v1/matches'):
    data=raw if raw is not None else json.dumps(body).encode() if body is not None else None
    req=urllib.request.Request(f'http://127.0.0.1:{port}'+path,data=data,headers={'Content-Type':'application/json'})
    try:
        with urllib.request.urlopen(req,timeout=4) as r:return r.status,json.loads(r.read())
    except urllib.error.HTTPError as e:
        raw=e.read()
        try:body=json.loads(raw)
        except ValueError:body={}
        return e.code,body

@contextlib.contextmanager
def service(executable):
    with tempfile.TemporaryDirectory(prefix='study-meta-109-') as directory:
        log=Path(directory)/'server.log'
        with log.open('w') as output:
            proc=subprocess.Popen([str(executable),'0'],stdout=output,stderr=output,cwd=directory)
        try:
            deadline=time.monotonic()+8;port=None
            while time.monotonic()<deadline:
                assert proc.poll() is None,'study service exited before readiness'
                lines=log.read_text().splitlines()
                for line in lines:
                    if line.startswith('PORT '):port=int(line.split()[1])
                if port:
                    try:
                        if request(port,path='/healthz')==(200,{'ok':True}):break
                    except OSError:pass
                time.sleep(.02)
            else:raise AssertionError('study service readiness timeout')
            yield port,proc
        finally:
            if proc.poll() is None:proc.terminate()
            try:proc.wait(timeout=3)
            except subprocess.TimeoutExpired:proc.kill();proc.wait(timeout=3)

def process_contract(build):
    server=build/'study_meta';client=build/'meta_submit'
    def submit(port,key):
        p=run([str(client),str(port),str(key)],timeout=10)
        assert f'confirmed key={key} ' in p.stdout,p.stdout
        return p.stdout
    with service(server) as (port,proc):
        a=submit(port,17);b=submit(port,17);assert a==b and 'row=1 ' in a
        assert 'row=2 ' in submit(port,18)
        record={'key':19,'round':1,'player_a':101,'player_b':202,'ticks':11,'score_a':100,'score_b':0,'lines_a':0,'lines_b':0,'winner':2}
        with ThreadPoolExecutor(max_workers=4) as workers:
            results=list(workers.map(lambda _:request(port,record),range(12)))
        assert all(r==results[0] for r in results) and results[0][0]==200 and results[0][1]['row']==3
        bad=dict(record,score_a=101);assert request(port,bad)[0]==409
        assert request(port,raw=b'{"bad":')[0]==400
        assert request(port,raw=b' '*1025)[0]==413
        assert request(port,path='/healthz')==(200,{'ok':True})
        assert proc.poll() is None
    with service(server) as (port,_):
        assert 'row=1 ' in submit(port,18) # New service lost its volatile rows.
    with service(server) as (port,proc):
        proc.terminate();proc.wait(timeout=3)
        p=subprocess.run([str(client),str(port),'17'],capture_output=True,text=True,timeout=8)
        assert p.returncode==3 and p.stdout.strip()=='unconfirmed'
    print('Separate processes: caller restart keeps service rows; service restart clears memory; duplicate/conflict/concurrency/413/unavailable contracts passed',flush=True)

def main():
    OUT.mkdir(parents=True,exist_ok=True)
    old=CP.parent/'108-meta-boundary'
    for p in old.rglob('*'):
        rel=p.relative_to(old)
        if p.is_file() and rel.as_posix() not in {'README.md','DESIGN.md','CMakeLists.txt'}:
            assert p.read_bytes()==(CP/rel).read_bytes(),rel
    if '--snippets-only' not in sys.argv:
        exe=OUT/'contract'
        run(['c++','-std=c++17','-pthread','-Wall','-Wextra','-Wpedantic','-fsanitize=address,undefined','-fno-sanitize-recover=all',
             '-I'+str(CP),'-isystem',str(ROOT/'third_party'),str(CP/'tests/meta_service_contract.cpp'),'-o',str(exe)])
        print(run([str(exe)],timeout=15).stdout,flush=True)
        for backend in ['SCRIPTED','SDL']:
            b=OUT/backend.lower()
            run(['cmake','-S',str(CP),'-B',str(b),'-DSTUDY_PLATFORM='+backend,'-DCMAKE_BUILD_TYPE=Release','-DCMAKE_CXX_FLAGS=-Wall -Wextra -Wpedantic'])
            p=run(['cmake','--build',str(b),'--target','study_meta','meta_submit','meta_service_contract','-j2'])
            (OUT/('build-'+backend+'.log')).write_text(p.stdout+p.stderr)
            assert 'warning:' not in p.stdout+p.stderr,p.stdout+p.stderr
            print(run(['ctest','--test-dir',str(b),'-R','^meta_service_contract$','--output-on-failure']).stdout,flush=True)
            process_contract(b)
    lesson=ROOT/'docs/learn/lessons/109.json'
    if lesson.exists():
        corpora={lang:[normalized(p.read_text(),lang) for p in CP.rglob('*') if p.is_file() and
            (p.suffix in ['.cpp','.h'] if lang=='cpp' else p.name=='CMakeLists.txt')] for lang in ['cpp','cmake']}
        count=0
        for section in json.loads(lesson.read_text())['sections']:
            for code in section.get('codes',[]):
                if code['language'] in corpora and 'text' in code:
                    assert any(normalized(code['text'],code['language']) in text for text in corpora[code['language']]),code['label']
                    count+=1
        print('inline snippets:',count)
if __name__=='__main__':main()
