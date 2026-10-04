"""Lesson175: actual C++ architecture boundaries without GUI/DB/model libraries."""
import argparse
import json
from pathlib import Path
from check_learning_text_layout import run
from check_part_docs import normalized
ROOT=Path(__file__).resolve().parents[1]
CP=ROOT/'docs/learn/checkpoints/175-change-boundaries'
OUT=ROOT/'out/learning-checkpoints/175-change-boundaries-check'


def snippets():
    previous=CP.with_name('174-release-evidence')
    for p in previous.rglob('*'):
        if p.is_file() and '__pycache__' not in p.parts:
            rel=p.relative_to(previous)
            if rel.as_posix() not in {'tests/character_art_contract.cpp', 'content/characters.h', 'roles/CMakeLists.txt', 'settings/config.h', 'content/art_set.h', 'tests/settings_contract.cpp', 'tests/widget_render.cpp', 'client/menu_model.h', 'README.md', 'renderer/menu_labels.h'}:
                assert p.read_bytes()==(CP/rel).read_bytes(),rel
    renamed = ['client/menu_model.h','content/art_set.h','content/characters.h',
               'renderer/menu_labels.h','settings/config.h','tests/character_art_contract.cpp',
               'tests/settings_contract.cpp','tests/widget_render.cpp']
    for rel in renamed:
        assert (CP/rel).read_text() == (previous/rel).read_text().replace(
            'study_characters', 'study_character_art'), rel
    lesson=ROOT/'docs/learn/lessons/175.json'
    corpus=[normalized(p.read_text(),'cpp') for p in CP.rglob('*') if p.suffix in {'.cpp','.h'}]
    count=0
    for section in json.loads(lesson.read_text())['sections']:
        ref=section.get('reference')
        if ref:assert ref['symbol'] in (ROOT/ref['path']).read_text(),ref
        for code in section.get('codes',[]):
            if 'text' in code and code['language']=='cpp':
                assert any(normalized(code['text'],'cpp') in text for text in corpus),code['label']
                count+=1
    print('Inline snippets:',count,'current symbols and cumulative preservation passed')


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--snippets-only',action='store_true');args=parser.parse_args()
    if not args.snippets_only:
        OUT.mkdir(parents=True,exist_ok=True)
        print(run(['cmake','-S',str(CP/'roles'),'-B',str(OUT),'-DSTUDY_ROLE=ARCHITECTURE',
                   '-DCMAKE_BUILD_TYPE=Release']).stdout,flush=True)
        print(run(['cmake','--build',str(OUT),'--config','Release','--target','architecture_contract','character_art_contract','settings_contract',
                   '--parallel','2']).stdout,flush=True)
        print(run(['ctest','--test-dir',str(OUT),'-C','Release','-R','^(architecture_contract|character_art_contract|settings_contract)$',
                   '--output-on-failure']).stdout,flush=True)
    snippets()


if __name__=='__main__':main()
