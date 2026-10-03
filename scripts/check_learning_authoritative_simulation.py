"""Lesson143: same rule core, trusted server identity and terminal-only result submission."""
from pathlib import Path
import argparse,json
from check_learning_text_layout import run
from check_part_docs import normalized
R=Path(__file__).resolve().parents[1]
CP=R/'docs/learn/checkpoints/143-authoritative-simulation'
OUT=R/'out/learning-checkpoints/143-authoritative-simulation-check'

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--boost',default='/usr/include')
    ap.add_argument('--snippets-only',action='store_true')
    args=ap.parse_args()
    previous=CP.parent/'142-recovery-journal'
    for p in previous.rglob('*'):
        if p.is_file() and p.relative_to(previous).as_posix() not in {'README.md','CMakeLists.txt'}:
            assert p.read_bytes()==(CP/p.relative_to(previous)).read_bytes(),p
    OUT.mkdir(parents=True,exist_ok=True)
    if not args.snippets_only:
        targets=['authoritative_contract','authoritative_probe','combat_contract','lockstep_contract','threat_model_contract','match_submission_contract']
        pattern='^('+'|'.join(targets+['epoll_authoritative_probe'])+')$'
        for backend in ['SCRIPTED','SDL']:
            b=OUT/backend.lower()
            run(['cmake','-S',str(CP),'-B',str(b),'-DSTUDY_PLATFORM='+backend,'-DSTUDY_AUDIO=NONE','-DCMAKE_BUILD_TYPE=Release','-DSTUDY_BOOST_INCLUDE='+str(Path(args.boost).resolve()),'-DCMAKE_CXX_FLAGS=-Wall -Wextra -Wpedantic'])
            built=run(['cmake','--build',str(b),'--target',*targets,'-j2'],timeout=300)
            (OUT/(backend.lower()+'-build.log')).write_text(built.stdout+built.stderr)
            print(backend,run(['ctest','--test-dir',str(b),'-R',pattern,'--output-on-failure'],timeout=90).stdout,flush=True)
        deps=['net/poll_reactor.cpp','net/epoll_reactor.cpp','net/socket.cpp','net/stream.cpp','net/send_socket.cpp','net/receive_socket.cpp']
        for name,source in [('contract','tests/authoritative_contract.cpp'),('probe','tools/authoritative_probe.cpp')]:
            exe=OUT/(name+'-sanitized')
            sources=[str(CP/source)]+([str(CP/p)for p in deps]if name=='probe'else[])
            built=run(['c++','-std=c++17','-O1','-g','-Wall','-Wextra','-Wpedantic','-fsanitize=address,undefined','-fno-sanitize-recover=all','-I'+str(CP),*sources,'-pthread','-o',str(exe)],timeout=180)
            (OUT/(name+'-sanitized-build.log')).write_text(built.stdout+built.stderr)
            for option in ([[],['--epoll']]if name=='probe'else[[]]):
                print('ASan/UBSan',run([str(exe),*option],timeout=30).stdout,flush=True)
        exe=OUT/'current-ranked-sanitized'
        run(['c++','-std=c++17','-O1','-g','-fsanitize=address,undefined','-fno-sanitize-recover=all','-I'+str(R),str(R/'tests/ranked_game_test.cpp'),str(R/'src/sim_game.cpp'),str(R/'src/position.cpp'),str(R/'net/framing.cpp'),'-o',str(exe)],timeout=180)
        run([str(exe)],timeout=30)
        print('Current RankedGame/direct SimGame oracle ASan/UBSan passed',flush=True)
    lesson=R/'docs/learn/lessons/143.json'
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
