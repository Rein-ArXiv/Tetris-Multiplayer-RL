"""Actual IOCP receiver: Windows cross-build, Linux API-double lifetime tests, cumulative code."""
from pathlib import Path
import json,sys,os,shutil
from check_learning_text_layout import run
from check_part_docs import normalized
ROOT=Path(__file__).resolve().parents[1];CP=ROOT/'docs/learn/checkpoints/128-iocp';OUT=ROOT/'out/learning-checkpoints/128-iocp-check'
def main():
 OUT.mkdir(parents=True,exist_ok=True);prev=CP.parent/'127-epoll'
 for p in prev.rglob('*'):
  if p.is_file() and p.relative_to(prev).as_posix()not in {'README.md','CMakeLists.txt'}:assert (CP/p.relative_to(prev)).read_bytes()==p.read_bytes(),p
 if '--snippets-only'not in sys.argv:
  for backend in ['SCRIPTED','SDL']:
   b=OUT/backend.lower();run(['cmake','-S',str(CP),'-B',str(b),f'-DSTUDY_PLATFORM={backend}','-DSTUDY_AUDIO=NONE','-DCMAKE_BUILD_TYPE=Release','-DCMAKE_CXX_FLAGS=-Wall -Wextra -Wpedantic'])
   targets=['completion_rules','epoll_contract','reactor_contract','reactor_probe']+(['tetris']if backend=='SDL'else[])
   r=run(['cmake','--build',str(b),'--target',*targets,'-j2']);(OUT/f'build-{backend}.log').write_text(r.stdout+r.stderr);print(backend,'targets built',flush=True)
   print(run(['ctest','--test-dir',str(b),'-R','^(reactor_contract|reactor_probe|epoll_shared_contract|epoll_frame_probe|epoll_contract|completion_rules)$','--output-on-failure'],timeout=60).stdout,flush=True)
  exe=OUT/'iocp-contract'
  run(['c++','-std=c++17','-D_WIN32','-I'+str(CP/'tests/fake_windows'),'-I'+str(CP),'-fsanitize=address,undefined','-fno-sanitize-recover=all',str(CP/'net/iocp_receive.cpp'),str(CP/'tests/iocp_contract.cpp'),'-o',str(exe)],timeout=120)
  for _ in range(20):r=run([str(exe)],timeout=10)
  print(r.stdout,flush=True);print('Full IOCP source/API-double contracts repeated20 with ASan/UBSan',flush=True)
  print(run([str(exe),'--broken-port'],env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'},timeout=10).stdout,flush=True)
  print('Only intentional retain-until-process-exit scenario disables leak detection; normal scenarios retain ASan leak checking',flush=True)
  print(run([sys.executable,str(ROOT/'scripts/check_learning_iocp_status.py')],timeout=60).stdout,flush=True)
  print(run([sys.executable,str(ROOT/'scripts/check_learning_iocp_lifetime.py')],timeout=60).stdout,flush=True)
  cross=os.environ.get('STUDY_MINGW_CXX') or shutil.which('x86_64-w64-mingw32-g++')
  if cross:
   common=[cross,'-std=c++17','-D_WIN32_WINNT=0x0601','-Wall','-Wextra','-Wpedantic']
   sources=['tools/iocp_probe.cpp','net/iocp_receive.cpp','net/socket.cpp','net/stream.cpp','net/send_socket.cpp','net/receive_socket.cpp']
   r=run([*common,'-I'+str(CP),*[str(CP/p)for p in sources],'-static','-lws2_32','-o',str(OUT/'iocp_probe.exe')],timeout=180)
   (OUT/'cross-probe.log').write_text(r.stdout+r.stderr)
   r=run([*common,'-I'+str(ROOT),str(ROOT/'tests/reactor_test.cpp'),str(ROOT/'net/reactor_iocp.cpp'),str(ROOT/'net/socket.cpp'),'-static','-lws2_32','-o',str(OUT/'root_reactor_test.exe')],timeout=180)
   (OUT/'cross-root.log').write_text(r.stdout+r.stderr)
   print('Windows PE cross-build: native IOCP frame probe and current reactor_test linked (not executed on Windows)',flush=True)
  else:print('Windows cross compiler unavailable; set STUDY_MINGW_CXX to validate native Windows build',flush=True)
 p=ROOT/'docs/learn/lessons/128.json'
 if p.exists():
  corpus={lang:[normalized(f.read_text(),lang)for f in CP.rglob('*')if f.is_file()and(f.suffix in ['.h','.cpp']if lang=='cpp'else f.name=='CMakeLists.txt')]for lang in ['cpp','cmake']};n=0
  for s in json.loads(p.read_text())['sections']:
   for c in s.get('codes',[]):
    if 'text'in c and c['language']in corpus:
     assert any(normalized(c['text'],c['language'])in t for t in corpus[c['language']]),c['label'];n+=1
  print('inline snippets',n,flush=True)
if __name__=='__main__':main()
