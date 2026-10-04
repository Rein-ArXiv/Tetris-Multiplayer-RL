"""Lesson169: real HTTP/SQLite startup, port boundaries and owned-child lifetime."""
from pathlib import Path
import argparse
import importlib.util
import json
import os
import subprocess
import sys
import tempfile
import textwrap
from check_learning_text_layout import run
from check_part_docs import normalized

ROOT=Path(__file__).resolve().parents[1]
CP=ROOT/'docs/learn/checkpoints/169-linux-service'
OUT=ROOT/'out/learning-checkpoints/169-linux-service-check'


def load_helper():
    spec=importlib.util.spec_from_file_location('study_service',CP/'operations/local_service.py')
    helper=importlib.util.module_from_spec(spec);spec.loader.exec_module(helper)
    return helper


def exchange(url, method, route, body=None, token=None):
    import http.client
    port=int(url.rsplit(':',1)[1])
    connection=http.client.HTTPConnection('127.0.0.1',port,timeout=2)
    headers={'Content-Type':'application/json'}
    if token:headers['Authorization']='Bearer '+token
    try:
        connection.request(method,route,body=body,headers=headers)
        response=connection.getresponse()
        return response.status,json.loads(response.read())
    finally:connection.close()


def actual_service(helper,binary):
    import socket
    with tempfile.TemporaryDirectory(prefix='study169 state ') as td:
        database=Path(td)/'account.sqlite'
        with helper.local_service(binary,database) as url:
            port=int(url.rsplit(':',1)[1])
            assert exchange(url,'GET','/healthz')==(200,{'ok':True})
            # Independent second listener must not steal the live endpoint.
            with socket.socket() as second:
                try:second.bind(('127.0.0.1',port))
                except OSError:pass
                else:raise AssertionError('an independent socket reused the occupied endpoint')
            status,identity=exchange(url,'POST','/study/v1/guest','{}')
            assert status==201 and identity['player_id']>0
        assert database.is_file()
        with helper.local_service(binary,database) as restarted:
            status,profile=exchange(restarted,'GET','/study/v1/me',token=identity['token'])
            assert status==200 and profile['player_id']==identity['player_id']
        with helper.local_service(binary,Path(td)/'other.sqlite') as separate:
            status,_=exchange(separate,'GET','/study/v1/me',token=identity['token'])
            assert status==401
        # Destroy only the caller-owned temporary fixture state; no user DB involved.
    print('Actual service: ephemeral bind, HTTP readiness, occupied port, separate DB and restart identity passed',flush=True)


def lifecycle_fixtures(helper):
    with tempfile.TemporaryDirectory(prefix='study169 lifecycle ') as td:
        base=Path(td)
        def check(name,body,expect=None,timeout=.8,raise_inside=False):
            executable=base/(name+'.py');pidfile=base/(name+'.pid')
            executable.write_text('#!/usr/bin/env python3\nimport os,time\nfrom pathlib import Path\nPath('+repr(str(pidfile))+').write_text(str(os.getpid()))\n'+textwrap.dedent(body))
            executable.chmod(0o700)
            try:
                with helper.local_service(executable,base/(name+'.db'),timeout=timeout):
                    if raise_inside:raise RuntimeError('caller failed')
                    if expect:raise AssertionError('unexpected readiness '+name)
            except RuntimeError as error:
                assert expect and expect in str(error),(name,str(error))
            pid=int(pidfile.read_text())
            try:os.kill(pid,0)
            except ProcessLookupError:pass
            else:raise AssertionError('owned child survived '+name)
        check('exit','raise SystemExit(7)','exited')
        check('silent','time.sleep(10)','deadline')
        check('bad_port','print("PORT 0",flush=True)\ntime.sleep(10)','invalid port')
        check('huge_port','print("x"*512,flush=True)\ntime.sleep(10)','oversized')
        server='''
        from http.server import HTTPServer,BaseHTTPRequestHandler
        class Handler(BaseHTTPRequestHandler):
            visits=0
            def log_message(self,*args):pass
            def do_GET(self):
                Handler.visits+=1
                body=BODY
                self.send_response(STATUS)
                self.send_header('Content-Length',str(len(body)))
                self.end_headers();self.wfile.write(body)
        server=HTTPServer(('127.0.0.1',0),Handler)
        print('PORT',server.server_port,flush=True)
        server.serve_forever()
        '''
        cases=[('oversized',b'{"ok":true}'+b' '*1100,'size limit'),
               ('encoding',b'\xff','valid JSON'),('shape',b'{"ok":1}','ok=true'),
               ('healthy',b'{"ok":true}',None)]
        for name,body,expect in cases:
            check(name,server.replace('BODY',repr(body)).replace('STATUS','200'),expect)
        check('caller_error',server.replace('BODY',repr(b'{"ok":true}')).replace('STATUS','200'),'caller failed',raise_inside=True)
        check('warming',server.replace('BODY',repr(b'{"ok":true}')).replace('STATUS','503 if Handler.visits == 1 else 200'))
        for value in (0,-1,float('nan'),float('inf')):
            try:
                with helper.local_service(base/'healthy.py',base/'x.db',timeout=value):pass
            except ValueError:pass
            else:raise AssertionError('invalid deadline accepted')
    print('Readiness failures, warming retry, bounded response and owned-child cleanup passed',flush=True)


def snippets():
    previous=CP.with_name('168-dependencies')
    for path in previous.rglob('*'):
        if path.is_file() and '__pycache__' not in path.parts and path.name!='README.md':
            assert (CP/path.relative_to(previous)).read_bytes()==path.read_bytes(),path
    lesson=ROOT/'docs/learn/lessons/169.json'
    if lesson.exists():
        corpus={language:[normalized(p.read_text(),language) for p in CP.rglob('*')
                           if p.is_file() and p.suffix in extensions]
                for language,extensions in [('cpp',{'.cpp','.h'}),('python',{'.py'})]}
        count=0
        for section in json.loads(lesson.read_text())['sections']:
            ref=section.get('reference')
            if ref:assert ref['symbol'] in (ROOT/ref['path']).read_text(),ref
            for code in section.get('codes',[]):
                if 'text' in code and code['language'] in corpus:
                    assert any(normalized(code['text'],code['language']) in s for s in corpus[code['language']]),code['label']
                    count+=1
        print('Inline snippets:',count,'current references and cumulative preservation passed')


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--snippets-only',action='store_true')
    args=parser.parse_args()
    if not args.snippets_only:
        assert sys.platform.startswith('linux'),'runtime audit is Linux-specific'
        print(run(['cmake','-S',str(CP/'roles'),'-B',str(OUT),'-DSTUDY_ROLE=SERVICE','-DCMAKE_BUILD_TYPE=Release']).stdout,flush=True)
        print(run(['cmake','--build',str(OUT),'--target','study_account_db','--parallel','2']).stdout,flush=True)
        helper=load_helper();actual_service(helper,OUT/'study_account_db');lifecycle_fixtures(helper)
    snippets()

if __name__=='__main__':main()
