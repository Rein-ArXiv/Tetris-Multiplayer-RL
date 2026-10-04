"""Lesson173: live WAL snapshot and isolated credential-version restore drill."""
import argparse
import json
from pathlib import Path
import subprocess
import sys
from check_learning_text_layout import run
from check_part_docs import normalized

ROOT=Path(__file__).resolve().parents[1]
CP=ROOT/'docs/learn/checkpoints/173-backup-restore'
OUT=ROOT/'out/learning-checkpoints/173-backup-restore-check'


def snippets():
    previous=CP.with_name('172-shutdown')
    for p in previous.rglob('*'):
        if p.is_file() and '__pycache__' not in p.parts:
            rel=p.relative_to(previous)
            if rel.as_posix()!='README.md':assert (CP/rel).read_bytes()==p.read_bytes(),rel
    assert (CP/'operations/sqlite_snapshot.py').read_bytes()==(ROOT/'scripts/backup_meta_db.py').read_bytes()
    manuscript=ROOT/'docs/learn/lessons/173.json'
    if not manuscript.exists():return
    corpus=[normalized(p.read_text(),'python') for p in (CP/'operations').glob('*.py')]
    count=0
    for section in json.loads(manuscript.read_text())['sections']:
        ref=section.get('reference')
        if ref:assert ref['symbol'] in (ROOT/ref['path']).read_text(),ref
        for code in section.get('codes',[]):
            if 'text' in code and code['language']=='python':
                assert any(normalized(code['text'],'python') in text for text in corpus),code['label']
                count+=1
    print('Inline snippets:',count,'current references, snapshot helper and cumulative preservation passed')


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--snippets-only',action='store_true')
    parser.add_argument('--binary',type=Path)
    args=parser.parse_args()
    if not args.snippets_only:
        OUT.mkdir(parents=True,exist_ok=True)
        binary=args.binary
        if binary is None:
            print(run(['cmake','-S',str(CP/'roles'),'-B',str(OUT),'-DSTUDY_ROLE=SERVICE',
                       '-DCMAKE_BUILD_TYPE=Release']).stdout,flush=True)
            print(run(['cmake','--build',str(OUT),'--config','Release','--target','study_account_db',
                       '--parallel','2']).stdout,flush=True)
            found=[p for p in [OUT/'study_account_db',OUT/'study_account_db.exe',
                               OUT/'Release/study_account_db.exe'] if p.is_file()]
            assert len(found)==1;binary=found[0]
        print(run([sys.executable,str(CP/'operations/recovery_drill.py'),'--binary',
                   str(binary.resolve())],timeout=30).stdout,flush=True)
    snippets()


if __name__=='__main__':main()
