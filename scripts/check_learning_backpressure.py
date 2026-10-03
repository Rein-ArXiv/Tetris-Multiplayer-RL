"""Bounded duplex relay: real sockets, shared reservations and current-source faults."""
from pathlib import Path
import json,sys,os,shutil
from check_learning_text_layout import run
from check_part_docs import normalized
R=Path(__file__).resolve().parents[1];CP=R/'docs/learn/checkpoints/132-backpressure';OUT=R/'out/learning-checkpoints/132-backpressure-check'
def main():
 OUT.mkdir(parents=True,exist_ok=True);prev=CP.parent/'131-state-machine'
 for p in prev.rglob('*'):
  if p.is_file()and p.relative_to(prev).as_posix()not in {'README.md','CMakeLists.txt'}:assert(CP/p.relative_to(prev)).read_bytes()==p.read_bytes(),p
 assert(CP/'net/byte_budget.h').read_text().replace('namespace study_net','namespace relay')==(R/'server/byte_budget.h').read_text()
 if '--snippets-only'not in sys.argv:
  exe=OUT/'contract-asan';run(['c++','-std=c++17','-pthread','-fsanitize=address,undefined','-fno-sanitize-recover=all','-I'+str(CP),str(CP/'tests/backpressure_contract.cpp'),'-o',str(exe)])
  print(run([str(exe)],timeout=15).stdout,flush=True)
  for backend in ['SCRIPTED','SDL']:
   b=OUT/backend.lower();run(['cmake','-S',str(CP),'-B',str(b),f'-DSTUDY_PLATFORM={backend}','-DSTUDY_AUDIO=NONE','-DCMAKE_BUILD_TYPE=Release','-DCMAKE_CXX_FLAGS=-Wall -Wextra -Wpedantic'])
   targets=['backpressure_contract','backpressure_probe','state_machine_contract','state_machine_probe','offload_contract','offload_probe','timer_contract','timer_probe','reactor_contract','reactor_probe','epoll_contract','completion_rules']+(['tetris']if backend=='SDL'else[])
   p=run(['cmake','--build',str(b),'--target',*targets,'-j2']);(OUT/f'build-{backend}.log').write_text(p.stdout+p.stderr)
   print(backend,run(['ctest','--test-dir',str(b),'-R','^(backpressure_contract|backpressure_probe|backpressure_stall|epoll_backpressure_probe|epoll_backpressure_stall|state_machine_contract|state_machine_probe|epoll_state_machine_probe|offload_contract|offload_probe|epoll_offload_probe|timer_contract|timer_probe|epoll_timer_probe|reactor_contract|reactor_probe|epoll_shared_contract|epoll_frame_probe|epoll_contract|completion_rules)$','--output-on-failure'],timeout=90).stdout,flush=True)
  sources=['tools/backpressure_probe.cpp','net/poll_reactor.cpp','net/epoll_reactor.cpp','net/socket.cpp','net/stream.cpp','net/send_socket.cpp','net/receive_socket.cpp']
  exe=OUT/'probe-asan';run(['c++','-std=c++17','-pthread','-fsanitize=address,undefined','-fno-sanitize-recover=all','-I'+str(CP),*[str(CP/f)for f in sources],'-o',str(exe)])
  for option in [[],['--epoll'],['--stall'],['--epoll','--stall']]:print(run([str(exe),*option],timeout=20).stdout,flush=True)
  print(run([sys.executable,str(R/'scripts/check_learning_backpressure_root.py')],timeout=60).stdout,flush=True)
  cross=os.environ.get('STUDY_MINGW_CXX')or shutil.which('x86_64-w64-mingw32-g++')
  if cross:
   for name,files in [('backpressure_probe',[f for f in sources if'epoll_reactor'not in f]),('backpressure_contract',['tests/backpressure_contract.cpp'])]:
    p=run([cross,'-std=c++17','-Wall','-Wextra','-Wpedantic','-I'+str(CP),*[str(CP/f)for f in files],'-static','-lws2_32','-o',str(OUT/(name+'.exe'))],timeout=180);(OUT/(name+'-cross.log')).write_text(p.stdout+p.stderr)
   print('Windows queue/relay cross-linked; no native execution',flush=True)
 p=R/'docs/learn/lessons/132.json'
 if p.exists():
  corpus={lang:[normalized(f.read_text(),lang)for f in CP.rglob('*')if f.is_file()and(f.suffix in ['.h','.cpp']if lang=='cpp'else f.name=='CMakeLists.txt')]for lang in ['cpp','cmake']};n=0
  for s in json.loads(p.read_text())['sections']:
   for c in s.get('codes',[]):
    if 'text'in c and c['language']in corpus:assert any(normalized(c['text'],c['language'])in t for t in corpus[c['language']]),c['label'];n+=1
  print('inline snippets',n,flush=True)
if __name__=='__main__':main()
