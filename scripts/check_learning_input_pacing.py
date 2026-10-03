"""Lesson144: exact integer pacing and layered input validation."""
from pathlib import Path
import argparse,json,random,subprocess
from check_learning_text_layout import run
from check_part_docs import normalized
R=Path(__file__).resolve().parents[1]
CP=R/'docs/learn/checkpoints/144-input-pacing'
OUT=R/'out/learning-checkpoints/144-input-pacing-check'

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--boost',default='/usr/include')
    ap.add_argument('--snippets-only',action='store_true')
    args=ap.parse_args()
    previous=CP.parent/'143-authoritative-simulation'
    for p in previous.rglob('*'):
        if p.is_file() and p.relative_to(previous).as_posix() not in {'README.md','CMakeLists.txt'}:
            assert p.read_bytes()==(CP/p.relative_to(previous)).read_bytes(),p
    OUT.mkdir(parents=True,exist_ok=True)
    if not args.snippets_only:
        targets=['pacing_contract','paced_probe','combat_contract','lockstep_contract','threat_model_contract','match_submission_contract']
        pattern='^('+'|'.join(targets+['epoll_paced_probe'])+')$'
        for backend in ['SCRIPTED','SDL']:
            b=OUT/backend.lower()
            run(['cmake','-S',str(CP),'-B',str(b),'-DSTUDY_PLATFORM='+backend,'-DSTUDY_AUDIO=NONE','-DCMAKE_BUILD_TYPE=Release','-DSTUDY_BOOST_INCLUDE='+str(Path(args.boost).resolve()),'-DCMAKE_CXX_FLAGS=-Wall -Wextra -Wpedantic'])
            built=run(['cmake','--build',str(b),'--target',*targets,'-j2'],timeout=300)
            (OUT/(backend.lower()+'-build.log')).write_text(built.stdout+built.stderr)
            print(backend,run(['ctest','--test-dir',str(b),'-R',pattern,'--output-on-failure'],timeout=90).stdout,flush=True)
        deps=['net/poll_reactor.cpp','net/epoll_reactor.cpp','net/socket.cpp','net/stream.cpp','net/send_socket.cpp','net/receive_socket.cpp']
        for name,source in [('contract','tests/pacing_contract.cpp'),('probe','tools/paced_probe.cpp')]:
            exe=OUT/(name+'-sanitized')
            sources=[str(CP/source)]+([str(CP/p)for p in deps]if name=='probe'else[])
            built=run(['c++','-std=c++17','-O1','-g','-Wall','-Wextra','-Wpedantic','-fsanitize=address,undefined','-fno-sanitize-recover=all','-I'+str(CP),*sources,'-pthread','-o',str(exe)],timeout=180)
            (OUT/(name+'-sanitized-build.log')).write_text(built.stdout+built.stderr)
            for option in ([[],['--epoll']]if name=='probe'else[[]]):
                print('ASan/UBSan',run([str(exe),*option],timeout=30).stdout,flush=True)
        randoms=random.Random(144)
        cases=[(r,l,m,t) for r in [1,3,60,999999999,1000000000]
               for m in [1,31,2**32] for l in [0,m//2,m]
               for t in [0,1,999999999,1000000000,2**63-1,2**64-1]]
        for _ in range(6000):
            m=randoms.randrange(1,2**32+1)
            cases.append((randoms.randrange(1,1000000001),randoms.randrange(m+1),m,randoms.randrange(2**64)))
        for _ in range(6000):
            maximum=randoms.randrange(1,2**32+1)
            lead=randoms.randrange(maximum+1)
            rate=randoms.randrange(1,1000000001)
            saturation=((maximum-lead)*10**9+rate-1)//rate
            cases.append((rate,lead,maximum,randoms.randrange(saturation+2)))
        # Concentrate additional samples around the next allowed tick.
        for r in [1,3,60,999999999,1000000000]:
            for n in [1,2,31,999,2**32-1]:
                boundary=(n*10**9+r-1)//r
                for delta in [-1,0,1]:cases.append((r,0,2**32,boundary+delta))
        payload=''.join(' '.join(map(str,c))+'\n' for c in cases)
        oracle=subprocess.run([str(OUT/'contract-sanitized'),'--oracle'],input=payload,text=True,capture_output=True,check=True)
        actual=list(map(int,oracle.stdout.split()))
        expected=[min(m,l+t*r//10**9) for r,l,m,t in cases]
        assert actual==expected
        print('Python arbitrary-precision oracle:',len(cases),'cases passed',flush=True)
        exe=OUT/'current-ranked-sanitized'
        run(['c++','-std=c++17','-O1','-g','-fsanitize=address,undefined','-fno-sanitize-recover=all','-I'+str(R),str(R/'tests/ranked_game_test.cpp'),str(R/'src/sim_game.cpp'),str(R/'src/position.cpp'),str(R/'net/framing.cpp'),'-o',str(exe)],timeout=180)
        run([str(exe)],timeout=30)
        print('Current RankedGame/direct SimGame oracle ASan/UBSan passed',flush=True)
    lesson=R/'docs/learn/lessons/144.json'
    if lesson.exists():
        corpus=[normalized(p.read_text(),'cpp')for p in CP.rglob('*')if p.suffix in {'.h','.cpp'}]
        count=0
        for s in json.loads(lesson.read_text())['sections']:
            ref=s.get('reference')
            if ref:assert ref['symbol']in(R/ref['path']).read_text(),ref
            for code in s.get('codes',[]):
                if 'text'in code and code['language']=='cpp':
                    assert any(normalized(code['text'],'cpp')in source for source in corpus),code['label']
                    count+=1
        print('Inline snippets',count,'current symbols and cumulative preservation passed',flush=True)
if __name__=='__main__':main()
