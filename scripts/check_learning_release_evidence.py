"""Lesson174: explicit scope/candidate evidence and actual local rules/service runs."""
import argparse
import json
from pathlib import Path
import sys
from check_learning_text_layout import run
from check_part_docs import normalized
ROOT=Path(__file__).resolve().parents[1]
CP=ROOT/'docs/learn/checkpoints/174-release-evidence'
OUT=ROOT/'out/learning-checkpoints/174-release-evidence-check'


def snippets():
    previous=CP.with_name('173-backup-restore')
    for p in previous.rglob('*'):
        if p.is_file() and '__pycache__' not in p.parts and p.name!='README.md':
            assert p.read_bytes()==(CP/p.relative_to(previous)).read_bytes(),p
    lesson=ROOT/'docs/learn/lessons/174.json'
    if not lesson.exists():return
    corpus=[normalized(p.read_text(),'python') for p in (CP/'operations').glob('*.py')]
    count=0
    for section in json.loads(lesson.read_text())['sections']:
        ref=section.get('reference')
        if ref:assert ref['symbol'] in (ROOT/ref['path']).read_text(),ref
        for code in section.get('codes',[]):
            if 'text' in code and code['language']=='python':
                assert any(normalized(code['text'],'python') in text for text in corpus),code['label']
                count+=1
    print('Inline snippets:',count,'current references and cumulative preservation passed')


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--snippets-only',action='store_true');args=parser.parse_args()
    print(run([sys.executable,str(CP/'operations/evidence_contract.py')]).stdout,flush=True)
    if not args.snippets_only:
        binaries={}
        for role,target in [('RULES','policy_match_contract'),('SERVICE','study_account_db')]:
            build=OUT/role.lower()
            print(run(['cmake','-S',str(CP/'roles'),'-B',str(build),'-DSTUDY_ROLE='+role,
                       '-DCMAKE_BUILD_TYPE=Release']).stdout,flush=True)
            print(run(['cmake','--build',str(build),'--config','Release','--target',target,
                       '--parallel','2']).stdout,flush=True)
            options=[build/target,build/(target+'.exe'),build/'Release'/(target+'.exe')]
            binaries[role]=next(p for p in options if p.exists())
        output=run([sys.executable,str(CP/'operations/release_drill.py'),'--rules',str(binaries['RULES']),
                    '--service',str(binaries['SERVICE'])],timeout=120).stdout
        report=json.loads(output);OUT.mkdir(parents=True,exist_ok=True)
        (OUT/'report.json').write_text(output)
        assert [r['status'] for r in report['records']]==['passed','passed'],report
        assert [d['status'] for d in report['assessment']['decisions']]==['passed','passed','not_run','not_run']
        assert report['assessment']['ready'] is False
        print('Actual rules and HTTP restore passed; GUI/load remain not_run. Report:',OUT/'report.json')
    snippets()


if __name__=='__main__':main()
