"""Lesson172: cooperative stop, owned socket release and signal-driven drain."""
import argparse
import json
import os
from pathlib import Path
import signal
import subprocess
import sys
import tempfile
import time
from check_learning_text_layout import run
from check_part_docs import normalized

ROOT=Path(__file__).resolve().parents[1]
CP=ROOT/'docs/learn/checkpoints/172-shutdown'
OUT=ROOT/'out/learning-checkpoints/172-shutdown-check'


def signal_stop(binary):
    modes = [signal.SIGTERM, signal.SIGINT] if os.name == 'posix' else [signal.CTRL_BREAK_EVENT]
    for sig in modes:
        with tempfile.TemporaryDirectory(prefix='study172-') as directory:
            output=Path(directory)/'output.log'
            flags=subprocess.CREATE_NEW_PROCESS_GROUP if os.name=='nt' else 0
            with output.open('wb') as stream:
                child=subprocess.Popen([str(binary)],stdout=stream,stderr=stream,creationflags=flags)
            try:
                deadline=time.monotonic()+5
                while b'READY\n' not in output.read_bytes().replace(b'\r\n',b'\n'):
                    assert child.poll() is None,output.read_text(errors='replace')
                    assert time.monotonic()<deadline,'readiness deadline'
                    time.sleep(.01)
                child.send_signal(sig)
                assert child.wait(timeout=5)==0,output.read_text(errors='replace')
                assert output.read_text().splitlines()==['READY','TASKS_DRAINED','PEER_EOF']
            finally:
                if child.poll() is None:child.kill();child.wait(timeout=3)
        print('Actual signal:',sig,'closed admission, capture/I/O drain, peer EOF passed',flush=True)


def snippets():
    previous=CP.with_name('171-private-publication')
    for p in previous.rglob('*'):
        if p.is_file() and '__pycache__' not in p.parts:
            rel=p.relative_to(previous)
            if rel.as_posix() not in {'README.md','roles/CMakeLists.txt'}:
                assert (CP/rel).read_bytes()==p.read_bytes(),rel
    manuscript=ROOT/'docs/learn/lessons/172.json'
    if not manuscript.exists():return
    corpus=[normalized(p.read_text(),'cpp') for p in CP.rglob('*') if p.suffix in {'.h','.cpp'}]
    count=0
    for section in json.loads(manuscript.read_text())['sections']:
        ref=section.get('reference')
        if ref:assert ref['symbol'] in (ROOT/ref['path']).read_text(),ref
        for code in section.get('codes',[]):
            if 'text' in code and code['language']=='cpp':
                assert any(normalized(code['text'],'cpp') in source for source in corpus),code['label']
                count+=1
    print('Inline snippets:',count,'current references and cumulative preservation passed',flush=True)


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--snippets-only',action='store_true')
    parser.add_argument('--binary',type=Path)
    args=parser.parse_args()
    if not args.snippets_only:
        OUT.mkdir(parents=True,exist_ok=True)
        binary=args.binary
        if binary is None:
            print(run(['cmake','-S',str(CP/'roles'),'-B',str(OUT/'build'),
                       '-DSTUDY_ROLE=SHUTDOWN','-DCMAKE_BUILD_TYPE=Release']).stdout,flush=True)
            print(run(['cmake','--build',str(OUT/'build'),'--config','Release','--parallel','2']).stdout,flush=True)
            choices=[OUT/'build/shutdown_demo',OUT/'build/shutdown_demo.exe',OUT/'build/Release/shutdown_demo.exe']
            print(run(['ctest','--test-dir',str(OUT/'build'),'-C','Release','--output-on-failure']).stdout,flush=True)
            found=[p for p in choices if p.is_file()];assert len(found)==1
            binary=found[0]
        binary=binary.resolve()
        assert run([str(binary),'--self-stop'],timeout=10).stdout.splitlines()==['READY','TASKS_DRAINED','PEER_EOF']
        signal_stop(binary)
        if sys.platform.startswith('linux'):
            for name,include,extra in [('study',CP,['-DSTUDY_WORKERS']),('current',ROOT,[])]:
                target=OUT/(name+'-workers')
                run(['c++','-std=c++17','-pthread','-Wall','-Wextra','-Wpedantic',
                     '-I'+str(include),*extra,str(ROOT/'tests/learning/worker_lifetime.cpp'),'-o',str(target)])
                print(run([str(target)],timeout=10).stdout,flush=True)
    snippets()


if __name__=='__main__':main()
