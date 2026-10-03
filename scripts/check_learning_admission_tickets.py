"""Lesson137 checkpoint/state checks; actual HTTP tests use the root meta target."""
from pathlib import Path
import argparse,json,os
from check_learning_text_layout import run
from check_part_docs import normalized
R=Path(__file__).resolve().parents[1];CP=R/'docs/learn/checkpoints/137-admission-tickets';OUT=R/'out/learning-checkpoints/137-admission-tickets-check'
def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--boost',default=os.environ.get('STUDY_BOOST_INCLUDE','/usr/include'))
    parser.add_argument('--snippets-only',action='store_true');args=parser.parse_args()
    prev=CP.parent/'136-wss-tunnel'
    for p in prev.rglob('*'):
        if p.is_file()and p.relative_to(prev).as_posix()not in {'CMakeLists.txt','README.md'}:
            assert (CP/p.relative_to(prev)).read_bytes()==p.read_bytes(),p
    OUT.mkdir(parents=True,exist_ok=True)
    if not args.snippets_only:
        boost=Path(args.boost).resolve();assert (boost/'boost/beast.hpp').exists(),'Set --boost or STUDY_BOOST_INCLUDE'
        b=OUT/'scripted'
        print(run(['cmake','-S',str(CP),'-B',str(b),'-DSTUDY_PLATFORM=SCRIPTED','-DSTUDY_AUDIO=NONE','-DCMAKE_BUILD_TYPE=Release',f'-DSTUDY_BOOST_INCLUDE={boost}']).stdout,flush=True)
        print(run(['cmake','--build',str(b),'--target','admission_ticket_contract','admission_ticket_demo','-j2'],timeout=180).stdout,flush=True)
        print(run(['ctest','--test-dir',str(b),'-R','^admission_ticket_(contract|demo)$','--output-on-failure']).stdout,flush=True)
        for label,source,include,libs in [('study',CP/'tests/admission_ticket_contract.cpp',CP,['-lcrypto']),('root',R/'tests/game_tickets_test.cpp',R,[])]:
            exe=OUT/(label+'-sanitized')
            run(['c++','-std=c++17','-pthread','-g','-Wall','-Wextra','-Wpedantic','-fsanitize=address,undefined','-fno-sanitize-recover=all','-I'+str(include),str(source),*libs,'-o',str(exe)],timeout=90)
            print('ASan/UBSan',label,run([str(exe)],timeout=30).stdout,flush=True)
    lesson=R/'docs/learn/lessons/137.json'
    if lesson.exists():
        corpus={lang:[normalized(p.read_text(),lang)for p in CP.rglob('*')if p.is_file()and(p.suffix in ['.cpp','.h']if lang=='cpp'else p.name=='CMakeLists.txt')]for lang in ['cpp','cmake']};count=0
        for section in json.loads(lesson.read_text())['sections']:
            r=section.get('reference')
            if r:assert r['symbol']in(R/r['path']).read_text(),r
            for code in section.get('codes',[]):
                if 'text'in code and code['language']in corpus:
                    assert any(normalized(code['text'],code['language'])in text for text in corpus[code['language']]),code['label'];count+=1
        print('Inline snippets',count,'current symbols and checkpoint inheritance verified',flush=True)
if __name__=='__main__':main()
