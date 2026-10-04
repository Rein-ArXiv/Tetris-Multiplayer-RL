"""Lesson170: Unicode DB paths, strict LF/CRLF announcements and native boundaries."""
import argparse
import importlib.util
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
from check_learning_text_layout import run
from check_part_docs import normalized
from check_learning_linux_service import exchange

ROOT=Path(__file__).resolve().parents[1]
CP=ROOT/'docs/learn/checkpoints/170-windows-port'
OUT=ROOT/'out/learning-checkpoints/170-windows-port-check'


def load_helper():
    spec=importlib.util.spec_from_file_location('study170_service',CP/'operations/local_service.py')
    helper=importlib.util.module_from_spec(spec);spec.loader.exec_module(helper)
    return helper


def parser_contract(helper):
    for ending in (b'\n',b'\r\n'):
        for port in (1,49152,65535):
            assert helper.parse_port_line(b'PORT '+str(port).encode()+ending)==port
    for line in (b'PORT 0\n',b'PORT 65536\n',b'PORT 01\n',b'PORT -1\n',b'PORT 1',
                 b'PORT 1\r',b'PORT 1\r\r\n',b' PORT 1\n',b'PORT 1 \n',b'PORT 1\nextra',
                 b'PORT 1\x00\n',b'PORT 123456\n'):
        try:helper.parse_port_line(line)
        except RuntimeError:pass
        else:raise AssertionError('invalid port announcement accepted')
    print('Strict LF/CRLF port announcement contract passed',flush=True)


def actual_service(helper,binary):
    with tempfile.TemporaryDirectory(prefix='study170 ') as temporary:
        state=Path(temporary)/'계정 경로 🎮';state.mkdir()
        database=state/'다른 작업 위치.sqlite'
        account=None
        for phase in ('create','reopen'):
            with helper.local_service(binary,database) as base:
                if phase=='create':
                    status,account=exchange(base,'POST','/study/v1/guest','{}')
                    assert status==201
                else:
                    status,profile=exchange(base,'GET','/study/v1/me',token=account['token'])
                    assert status==200 and profile['player_id']==account['player_id']
                assert database.is_file(),'the exact Unicode database path must be used'
    print('Actual account service: Unicode/space path, changed cwd, restart identity passed',flush=True)


def snippets():
    previous=CP.with_name('169-linux-service')
    changed={'README.md','meta/account_service.cpp','roles/CMakeLists.txt','operations/local_service.py'}
    for path in previous.rglob('*'):
        if path.is_file() and '__pycache__' not in path.parts:
            rel=path.relative_to(previous)
            if rel.as_posix() not in changed:assert (CP/rel).read_bytes()==path.read_bytes(),rel
    for name in ('utf8_arguments.h','utf8_arguments.cpp'):
        assert (CP/'platform'/name).read_bytes()==(ROOT/'platform'/name).read_bytes()
    manuscript=ROOT/'docs/learn/lessons/170.json'
    if not manuscript.exists():return
    corpus={language:[normalized(p.read_text(),language) for p in CP.rglob('*')
                      if p.is_file() and p.suffix in extensions]
            for language,extensions in [('cpp',{'.cpp','.h'}),('python',{'.py'}),('cmake',{'.txt','.cmake'})]}
    count=0
    for section in json.loads(manuscript.read_text())['sections']:
        ref=section.get('reference')
        if ref:assert ref['symbol'] in (ROOT/ref['path']).read_text(),ref
        for code in section.get('codes',[]):
            if 'text' in code and code['language'] in corpus:
                assert any(normalized(code['text'],code['language']) in source for source in corpus[code['language']]),code['label']
                count+=1
    print('Inline snippets:',count,'current references and cumulative preservation passed')


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--snippets-only',action='store_true')
    parser.add_argument('--binary',type=Path)
    args=parser.parse_args()
    if not args.snippets_only:
        helper=load_helper();parser_contract(helper)
        binary=args.binary
        if binary is None:
            print(run(['cmake','-S',str(CP/'roles'),'-B',str(OUT),'-DSTUDY_ROLE=SERVICE','-DCMAKE_BUILD_TYPE=Release']).stdout,flush=True)
            print(run(['cmake','--build',str(OUT),'--config','Release','--target','study_account_db','--parallel','2']).stdout,flush=True)
            choices=[OUT/'Release/study_account_db.exe',OUT/'study_account_db.exe',OUT/'study_account_db']
            binaries=[p for p in choices if p.is_file()]
            assert len(binaries)==1,'provide --binary when output layouts are ambiguous'
            binary=binaries[0]
        actual_service(helper,binary.resolve())
        print('Runtime OS:',sys.platform,'; other OS native execution is separate',flush=True)
    snippets()

if __name__=='__main__':main()
