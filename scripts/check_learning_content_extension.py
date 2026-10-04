"""Lesson176: stable character registration, local pacing and exact content package."""
import argparse
import hashlib
import json
from pathlib import Path
import sys
import tempfile
import zipfile
from check_learning_text_layout import run
from check_part_docs import normalized
ROOT=Path(__file__).resolve().parents[1]
CP=ROOT/'docs/learn/checkpoints/176-content-extension'
OUT=ROOT/'out/learning-checkpoints/176-content-extension-check'


def snippets():
    previous=CP.with_name('175-change-boundaries')
    for p in previous.rglob('*'):
        if p.is_file() and '__pycache__' not in p.parts:
            rel=p.relative_to(previous)
            if rel.as_posix() not in {'README.md','roles/CMakeLists.txt'}:
                assert p.read_bytes()==(CP/rel).read_bytes(),rel
    lesson=ROOT/'docs/learn/lessons/176.json'
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
    print('Inline snippets:',count,'current references and cumulative preservation passed')


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--snippets-only',action='store_true');args=parser.parse_args()
    if not args.snippets_only:
        OUT.mkdir(parents=True,exist_ok=True)
        print(run(['cmake','-S',str(CP/'roles'),'-B',str(OUT),'-DSTUDY_ROLE=CONTENT',
                   '-DCMAKE_BUILD_TYPE=Release']).stdout,flush=True)
        print(run(['cmake','--build',str(OUT),'--config','Release','--parallel','2']).stdout,flush=True)
        print(run(['ctest','--test-dir',str(OUT),'-C','Release','--output-on-failure']).stdout,flush=True)
        options=[OUT/'content_demo',OUT/'content_demo.exe',OUT/'Release/content_demo.exe']
        binary=next(p for p in options if p.exists())
        for ident in ['rook','mira']:
            output=run([str(binary),str(CP/'assets/opponents.cfg'),ident,'150']).stdout
            assert f'selected "{ident}" reward_eligible 0' in output and 'decision 1' in output,output
            (OUT/(ident+'.log')).write_text(output)
        with tempfile.TemporaryDirectory(prefix='study176-') as temp:
            out=Path(temp)/'opponents.zip'
            print(run([sys.executable,str(CP/'operations/package_content.py'),'--root',str(CP),'--out',str(out)]).stdout)
            # Parse the exact archived configuration using the shared field contract.
            sys.path.insert(0,str(CP/'operations'))
            from opponent_profile import split_profile_line
            required={'assets/opponents.cfg'}
            for line in (CP/'assets/opponents.cfg').read_text().split('\n'):
                fields=split_profile_line(line)
                if fields:required.update(p for p in fields[2:5] if p and p!='@heuristic')
            with zipfile.ZipFile(out) as archive:
                assert set(archive.namelist())==required|{'opponents-manifest.json'}
                manifest=json.loads(archive.read('opponents-manifest.json'))
                assert set(manifest['sha256'])==required
                for name in required:
                    data=archive.read(name)
                    assert data==(CP/name).read_bytes()
                    assert hashlib.sha256(data).hexdigest()==manifest['sha256'][name]
        print('Both profiles exercised decisions; referenced assets and manifest match; practice has no reward')
    snippets()


if __name__=='__main__':main()
