"""Lesson146: result meaning, complete FIFO notification and bounded drain."""
from pathlib import Path
import argparse,json
from check_learning_text_layout import run
from check_part_docs import normalized
R=Path(__file__).resolve().parents[1]
CP=R/'docs/learn/checkpoints/146-result-notices'
OUT=R/'out/learning-checkpoints/146-result-notices-check'
def main():
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('--boost',default='/usr/include');ap.add_argument('--snippets-only',action='store_true');args=ap.parse_args()
    previous=CP.parent/'145-relay-ownership'
    for p in previous.rglob('*'):
        if p.is_file() and p.relative_to(previous).as_posix() not in {'README.md','CMakeLists.txt'}:
            assert p.read_bytes()==(CP/p.relative_to(previous)).read_bytes(),p
    OUT.mkdir(parents=True,exist_ok=True)
    if not args.snippets_only:
        targets=['result_notice_contract','notice_probe','relay_owner_contract','pacing_contract','match_submission_contract']
        for backend in ['SCRIPTED','SDL']:
            b=OUT/backend.lower()
            run(['cmake','-S',str(CP),'-B',str(b),'-DSTUDY_PLATFORM='+backend,'-DSTUDY_AUDIO=NONE','-DCMAKE_BUILD_TYPE=Release','-DSTUDY_BOOST_INCLUDE='+str(Path(args.boost).resolve()),'-DCMAKE_CXX_FLAGS=-Wall -Wextra -Wpedantic'])
            result=run(['cmake','--build',str(b),'--target',*targets,'-j2'],timeout=300);(OUT/(backend.lower()+'-build.log')).write_text(result.stdout+result.stderr)
            print(backend,run(['ctest','--test-dir',str(b),'-R','^('+'|'.join(targets+['epoll_notice_probe'])+')$','--no-tests=error','--output-on-failure'],timeout=90).stdout,flush=True)
        deps=['net/poll_reactor.cpp','net/epoll_reactor.cpp','net/socket.cpp','net/stream.cpp','net/send_socket.cpp','net/receive_socket.cpp']
        for name,source in [('contract','tests/result_notice_contract.cpp'),('probe','tools/notice_probe.cpp')]:
            exe=OUT/(name+'-sanitized');sources=[str(CP/source)]+([str(CP/p)for p in deps]if name=='probe'else[])
            result=run(['c++','-std=c++17','-O1','-g','-Wall','-Wextra','-Wpedantic','-fsanitize=address,undefined','-fno-sanitize-recover=all','-I'+str(CP),*sources,'-pthread','-o',str(exe)],timeout=180);(OUT/(name+'-sanitized-build.log')).write_text(result.stdout+result.stderr)
            for option in ([[],['--epoll']]if name=='probe'else[[]]):print('ASan/UBSan',run([str(exe),*option],timeout=30).stdout,flush=True)
        exe=OUT/'current-status'
        run(['c++','-std=c++17','-fsanitize=address,undefined','-I'+str(R),str(R/'tests/learning/match_result_status.cpp'),'-o',str(exe)])
        print(run([str(exe)]).stdout,flush=True)
    lesson=R/'docs/learn/lessons/146.json'
    if lesson.exists():
        corpus=[normalized(p.read_text(),'cpp')for p in CP.rglob('*')if p.suffix in {'.h','.cpp'}];count=0
        for s in json.loads(lesson.read_text())['sections']:
            ref=s.get('reference')
            if ref:assert ref['symbol']in(R/ref['path']).read_text(),ref
            for code in s.get('codes',[]):
                if 'text'in code and code['language']=='cpp':assert any(normalized(code['text'],'cpp')in source for source in corpus),code['label'];count+=1
        print('Inline snippets',count,'current symbols and cumulative preservation passed',flush=True)
if __name__=='__main__':main()
