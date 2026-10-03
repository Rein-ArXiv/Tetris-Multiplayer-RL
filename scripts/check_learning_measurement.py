"""Bounded loopback workload: measurements are observations, never speed gates."""
from pathlib import Path
import json,sys,math,subprocess,platform
from check_learning_text_layout import run
from check_part_docs import normalized
ROOT=Path(__file__).resolve().parents[1]
CP=ROOT/'docs/learn/checkpoints/124-thread-measurement'
OUT=ROOT/'out/learning-checkpoints/124-thread-measurement-check'

def exercise(exe):
    records=[]
    for repetition in range(3):
        for poll,pairs in ([(1,1),(5,1),(1,4),(5,4),(1,16),(5,16)] if repetition%2==0 else [(5,16),(1,16),(5,4),(1,4),(5,1),(1,1)]):
            r=run([str(exe),str(pairs),'100',str(poll)],timeout=65)
            x=json.loads(r.stdout);x['repetition']=repetition;records.append(x)
            assert x['status']=='ok' and x['started']==x['completed']==x['planned']==pairs*100
            assert x['incomplete']==x['unattempted']==0 and len(x['samples_us'])==x['completed']
            v=sorted(x['samples_us']);s=x['latency']
            for key,want in [('p50_us',v[math.ceil(len(v)*.50)-1]),('p99_us',v[math.ceil(len(v)*.99)-1]),('max_us',v[-1]),('mean_us',sum(v)/len(v))]:
                assert math.isclose(s[key],want,rel_tol=1e-9), (key,s[key],want)
            for key in ['idle','active']:
                w=x[key];assert w['wall_seconds']>0 and w['cpu_seconds']>=0
                assert math.isclose(w['cpu_core_equivalents'],w['cpu_seconds']/w['wall_seconds'],rel_tol=1e-8,abs_tol=1e-9)
            assert math.isclose(x['completed_per_second'],x['completed']/x['active']['wall_seconds'],rel_tol=1e-8)
            print(f"Observed rep={repetition} pairs={pairs} poll={poll}ms p50={s['p50_us']:.1f}us p99={s['p99_us']:.1f}us idle_cpu={x['idle']['cpu_core_equivalents']:.4f}core",flush=True)
    (OUT/'observations.json').write_text(json.dumps(dict(environment=dict(system=platform.system(),release=platform.release(),machine=platform.machine()),runs=records),indent=2)+'\n')
    for mode,status in [('drop','timeout'),('corrupt','bad_reply')]:
        r=subprocess.run([str(exe),'4','10','1',mode],capture_output=True,text=True,timeout=10)
        x=json.loads(r.stdout);assert r.returncode==1 and x['status']==status
        assert x['started']==4 and x['completed']==0 and x['incomplete']==4 and x['unattempted']==36
        assert x['latency'] is None and x['samples_us']==[]
    for args in [('0','2','1'),('17','2','1'),('1','0','1'),('1','1001','1'),('1','2','0'),('1','2','11'),('1junk','2','1'),('1','2','1','bad')]:
        assert subprocess.run([str(exe),*args],capture_output=True,timeout=5).returncode==2
    print('Loopback: all replies verified; missing/corrupt replies fail; denominator and raw percentiles checked',flush=True)

def main():
    OUT.mkdir(parents=True,exist_ok=True)
    prev=CP.parent/'123-ranking';allowed={'README.md','CMakeLists.txt','net/thread_link.h','net/thread_link.cpp'}
    for p in prev.rglob('*'):
        if p.is_file() and p.relative_to(prev).as_posix() not in allowed:
            assert (CP/p.relative_to(prev)).read_bytes()==p.read_bytes(),p
    if '--snippets-only' not in sys.argv:
        for backend in ['SCRIPTED','SDL']:
            b=OUT/backend.lower()
            run(['cmake','-S',str(CP),'-B',str(b),f'-DSTUDY_PLATFORM={backend}','-DSTUDY_AUDIO=NONE','-DCMAKE_BUILD_TYPE=Release','-DCMAKE_CXX_FLAGS=-Wall -Wextra -Wpedantic'])
            targets=['thread_measure','measurement_contract','thread_contract','flow_worker']
            if backend=='SDL':targets+=['tetris']
            r=run(['cmake','--build',str(b),'--target',*targets,'-j2']);(OUT/f'build-{backend}.log').write_text(r.stdout+r.stderr)
            print(backend,'targets built',flush=True)
            print(run(['ctest','--test-dir',str(b),'-R','^(measurement_contract|thread_contract|flow_worker)$','--output-on-failure'],timeout=60).stdout,flush=True)
            if backend=='SCRIPTED':exercise(b/'thread_measure')
        sources=['net/socket.cpp','net/stream.cpp','net/connection.cpp','net/send_socket.cpp','net/receive_socket.cpp','net/thread_link.cpp']
        exe=OUT/'measurement-sanitized'
        run(['c++','-std=c++17','-pthread','-fsanitize=address,undefined','-fno-sanitize-recover=all','-I'+str(CP),str(CP/'tests/measurement_contract.cpp'),*[str(CP/p)for p in sources],'-o',str(exe)],timeout=120)
        print(run([str(exe)],timeout=20).stdout,flush=True)
        probe=OUT/'thread-measure-sanitized'
        run(['c++','-std=c++17','-pthread','-fsanitize=address,undefined','-fno-sanitize-recover=all','-I'+str(CP),str(CP/'tools/thread_measure.cpp'),*[str(CP/p)for p in sources],'-o',str(probe)],timeout=120)
        sample=json.loads(run([str(probe),'4','10','5'],timeout=20).stdout)
        assert sample['status']=='ok' and sample['completed']==40
        print('ASan/UBSan: complete socket worker path and measured echo passed',flush=True)
    lesson=ROOT/'docs/learn/lessons/124.json'
    if lesson.exists():
        corpora={lang:[normalized(p.read_text(),lang)for p in CP.rglob('*')if p.is_file()and(p.suffix in ['.cpp','.h']if lang=='cpp'else p.name=='CMakeLists.txt')]for lang in ['cpp','cmake']}
        n=0
        for sec in json.loads(lesson.read_text())['sections']:
            for code in sec.get('codes',[]):
                if 'text'in code and code['language']in corpora:
                    assert any(normalized(code['text'],code['language'])in c for c in corpora[code['language']]),code['label'];n+=1
        print('inline snippets',n,flush=True)
if __name__=='__main__':main()
