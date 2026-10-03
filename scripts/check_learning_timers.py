"""Timer heap: synthetic-time model, cumulative poll/epoll loop and source snippets."""
from pathlib import Path
import json,sys,os,shutil
from check_learning_text_layout import run
from check_part_docs import normalized
ROOT=Path(__file__).resolve().parents[1];CP=ROOT/'docs/learn/checkpoints/129-timers';OUT=ROOT/'out/learning-checkpoints/129-timers-check'
def main():
 OUT.mkdir(parents=True,exist_ok=True)
 prev=CP.parent/'128-iocp'
 for p in prev.rglob('*'):
  if p.is_file() and p.relative_to(prev).as_posix()not in {'README.md','CMakeLists.txt'}:assert (CP/p.relative_to(prev)).read_bytes()==p.read_bytes(),p
 if '--snippets-only'not in sys.argv:
  exe=OUT/'timer-contract-asan'
  run(['c++','-std=c++17','-fsanitize=address,undefined','-fno-sanitize-recover=all','-I'+str(CP),str(CP/'tests/timer_contract.cpp'),'-o',str(exe)])
  print(run([str(exe)]).stdout,flush=True)
  for backend in ['SCRIPTED','SDL']:
   b=OUT/backend.lower();run(['cmake','-S',str(CP),'-B',str(b),f'-DSTUDY_PLATFORM={backend}','-DSTUDY_AUDIO=NONE','-DCMAKE_BUILD_TYPE=Release','-DCMAKE_CXX_FLAGS=-Wall -Wextra -Wpedantic'])
   targets=['timer_contract','timer_probe','reactor_contract','reactor_probe','epoll_contract','completion_rules']+(['tetris']if backend=='SDL'else[])
   r=run(['cmake','--build',str(b),'--target',*targets,'-j2']);(OUT/f'build-{backend}.log').write_text(r.stdout+r.stderr);print(backend,'targets built',flush=True)
   print(run(['ctest','--test-dir',str(b),'-R','^(timer_contract|timer_probe|epoll_timer_probe|reactor_contract|reactor_probe|epoll_shared_contract|epoll_frame_probe|epoll_contract|completion_rules)$','--output-on-failure'],timeout=60).stdout,flush=True)
  cross=os.environ.get('STUDY_MINGW_CXX') or shutil.which('x86_64-w64-mingw32-g++')
  if cross:
   sources=['tools/timer_probe.cpp','net/poll_reactor.cpp','net/socket.cpp','net/stream.cpp','net/send_socket.cpp','net/receive_socket.cpp']
   for name,files in [('timer_probe',sources),('timer_contract',['tests/timer_contract.cpp'])]:
    r=run([cross,'-std=c++17','-Wall','-Wextra','-Wpedantic','-I'+str(CP),*[str(CP/f)for f in files],'-static','-lws2_32','-o',str(OUT/(name+'.exe'))],timeout=180)
    (OUT/(name+'-cross.log')).write_text(r.stdout+r.stderr)
   print('Windows timer probe/model cross-build linked; not executed on Windows',flush=True)
 p=ROOT/'docs/learn/lessons/129.json'
 if p.exists():
  corpus={lang:[normalized(f.read_text(),lang)for f in CP.rglob('*')if f.is_file()and(f.suffix in ['.h','.cpp']if lang=='cpp'else f.name=='CMakeLists.txt')]for lang in ['cpp','cmake']};n=0
  for s in json.loads(p.read_text())['sections']:
   for c in s.get('codes',[]):
    if 'text'in c and c['language']in corpus:
     assert any(normalized(c['text'],c['language'])in t for t in corpus[c['language']]),c['label'];n+=1
  print('inline snippets',n,flush=True)
if __name__=='__main__':main()
