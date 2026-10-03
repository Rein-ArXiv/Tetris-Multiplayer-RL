"""Lesson 93: bounded value handoffs and a socket owned by one worker."""
from pathlib import Path
import os,sys,subprocess
from check_learning_text_layout import run
from check_learning_framing import finish,line
from check_learning_seed import cleanup
from check_learning_utf8 import cut
from check_part_docs import normalized
import json
ROOT=Path(__file__).resolve().parents[1]
CP=ROOT/'docs/learn/checkpoints/93-thread-queues'
OUT=ROOT/'out/learning-checkpoints/93-thread-queues-check'
def main():
 OUT.mkdir(parents=True,exist_ok=True)
 prev=CP.parent/'92-hash-audit'
 for p in prev.rglob('*'):
  if p.is_file() and p.relative_to(prev).as_posix() not in {'README.md','DESIGN.md','CMakeLists.txt'}:
   assert p.read_bytes()==(CP/p.relative_to(prev)).read_bytes(),p
 if '--skip-build' not in sys.argv:
  for backend in ['SCRIPTED','SDL']:
   b=OUT/backend.lower()
   run(['cmake','-S',str(CP),'-B',str(b),'-DSTUDY_PLATFORM='+backend,'-DCMAKE_BUILD_TYPE=Release','-DCMAKE_CXX_FLAGS=-Wall -Wextra -Wpedantic'])
   p=run(['cmake','--build',str(b),'-j3']);(OUT/f'build-{backend}.log').write_text(p.stdout+p.stderr)
   assert 'warning:' not in p.stdout+p.stderr,p.stderr
   p=run(['ctest','--test-dir',str(b),'--output-on-failure'],env={**os.environ,'SDL_VIDEODRIVER':'dummy','SDL_AUDIODRIVER':'dummy','DISPLAY':'','WAYLAND_DISPLAY':''})
   print(backend,p.stdout[-220:],flush=True)
 flags=['c++','-std=c++17','-Wall','-Wextra','-Wpedantic','-pthread','-fsanitize=address,undefined','-fno-sanitize-recover=all','-I'+str(CP)]
 sources=[CP/'tests/thread_contract.cpp',*[CP/f'net/{f}.cpp' for f in ['socket','stream','send_socket','receive_socket','thread_link']]]
 run([*flags,*map(str,sources),'-o',str(OUT/'thread-sanitized')])
 print(run([str(OUT/'thread-sanitized')],timeout=25).stdout,flush=True)
 run([*flags,str(CP/'tests/thread_policy.cpp'),str(CP/'net/thread_link.cpp'),str(CP/'net/socket.cpp'),'-o',str(OUT/'policy-sanitized')])
 print(run([str(OUT/'policy-sanitized')],timeout=15).stdout,flush=True)
 for n in range(4):
  p=subprocess.Popen([str(OUT/'scripted/thread_probe'),'listen','0'],stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True)
  try:
   ready=line(p);assert ready.startswith('LISTEN ')
   peer=run([str(OUT/'scripted/thread_probe'),'connect',ready.split()[1]],timeout=10)
   host=finish(p,0)[0];assert 'DONE frames=4 end=0' in host and 'DONE frames=4 end=0' in peer.stdout
  finally:cleanup(p)
 print('Four independent process exchanges passed',flush=True)
 lesson=ROOT/'docs/learn/lessons/093.json'
 if lesson.exists():
  corpus={lang:'\n'.join(p.read_text() for p in CP.rglob('*') if p.is_file() and (p.suffix in ['.h','.cpp'] if lang=='cpp' else p.name=='CMakeLists.txt')) for lang in ['cpp','cmake']}
  count=0
  for s in json.loads(lesson.read_text())['sections']:
   for code in s.get('codes',[]):
    if 'file' not in code and code['language'] in corpus:
     assert normalized(code['text'],code['language']) in normalized(corpus[code['language']],code['language']),code['label'];count+=1
  print('Inline implementation snippets:',count,flush=True)
 print('Thread queue checks complete',flush=True)
if __name__=='__main__':main()
