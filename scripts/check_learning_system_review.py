"""Lesson177: cumulative role contracts and actual isolated service restoration."""
import argparse
import json
from pathlib import Path
import sys
from check_learning_text_layout import run
from check_part_docs import normalized
ROOT=Path(__file__).resolve().parents[1]
CP=ROOT/'docs/learn/checkpoints/177-system-review'
OUT=ROOT/'out/learning-checkpoints/177-system-review-check'


def snippets():
    previous=CP.with_name('176-content-extension')
    for p in previous.rglob('*'):
        if p.is_file() and '__pycache__' not in p.parts:
            rel=p.relative_to(previous)
            if rel.as_posix()!='README.md':assert p.read_bytes()==(CP/rel).read_bytes(),rel
    lesson=ROOT/'docs/learn/lessons/177.json'
    if not lesson.exists():return
    corpus={lang:[normalized(p.read_text(),lang) for p in CP.rglob('*') if p.suffix in suffixes]
            for lang,suffixes in [('cpp',{'.cpp','.h'}),('python',{'.py'})]}
    count=0
    for section in json.loads(lesson.read_text())['sections']:
        ref=section.get('reference')
        if ref:assert ref['symbol'] in (ROOT/ref['path']).read_text(),ref
        for code in section.get('codes',[]):
            lang=code['language']
            if 'text' in code and lang in corpus:
                assert any(normalized(code['text'],lang) in text for text in corpus[lang]),code['label']
                count+=1
    print('Inline snippets:',count,'current references and final cumulative preservation passed')


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--snippets-only',action='store_true');args=parser.parse_args()
    if not args.snippets_only:
        OUT.mkdir(parents=True,exist_ok=True)
        for role in ['RULES','ARCHITECTURE','CONTENT','SHUTDOWN','SERVICE']:
            build=OUT/role.lower()
            print(role,run(['cmake','-S',str(CP/'roles'),'-B',str(build),'-DSTUDY_ROLE='+role,
                       '-DCMAKE_BUILD_TYPE=Release']).stdout,flush=True)
            print(run(['cmake','--build',str(build),'--config','Release','--parallel','2']).stdout,flush=True)
            if role!='SERVICE':
                print(run(['ctest','--test-dir',str(build),'-C','Release','--no-tests=error',
                           '--output-on-failure'],timeout=90).stdout,flush=True)
            else:
                options=[build/'study_account_db',build/'study_account_db.exe',build/'Release/study_account_db.exe']
                binary=next(p for p in options if p.is_file())
                print(run([sys.executable,str(CP/'operations/recovery_drill.py'),'--binary',str(binary)],timeout=90).stdout,flush=True)
        print(run([sys.executable,str(CP/'operations/evidence_contract.py')]).stdout,flush=True)
        print('Cumulative roles verified; GUI pixels, training performance and production capacity are separate evidence')
    snippets()


if __name__=='__main__':main()
