"""Lesson87: cooperative send policy, real backpressure and production deadline."""
from pathlib import Path
import os,sys
from check_learning_text_layout import run
import check_learning_serialization as codec
ROOT=Path(__file__).resolve().parents[1]
CP=ROOT/'docs/learn/checkpoints/87-partial-send'
OUT=ROOT/'out/learning-checkpoints/87-partial-send-check'
def main():
 OUT.mkdir(parents=True,exist_ok=True)
 prev=CP.parent/'86-serialization'
 for p in prev.rglob('*'):
  if p.is_file() and p.relative_to(prev).as_posix() not in {'README.md','DESIGN.md','CMakeLists.txt','tools/serialization_probe.cpp'}:assert p.read_bytes()==(CP/p.relative_to(prev)).read_bytes(),p
 if '--skip-build' not in sys.argv:
  for backend in ['SCRIPTED','SDL']:
   build=OUT/backend.lower();run(['cmake','-S',str(CP),'-B',str(build),'-DSTUDY_PLATFORM='+backend,'-DCMAKE_BUILD_TYPE=Release','-DCMAKE_CXX_FLAGS=-Wall -Wextra -Wpedantic'])
   r=run(['cmake','--build',str(build),'-j3']);(OUT/f'build-{backend}.log').write_text(r.stdout+r.stderr);assert 'warning:' not in r.stdout+r.stderr
   r=run(['ctest','--test-dir',str(build),'--output-on-failure'],env={**os.environ,'SDL_VIDEODRIVER':'dummy','SDL_AUDIODRIVER':'dummy','DISPLAY':'','WAYLAND_DISPLAY':''});(OUT/f'ctest-{backend}.log').write_text(r.stdout)
 flags=['c++','-std=c++17','-Wall','-Wextra','-Wpedantic','-fsanitize=address,undefined','-fno-sanitize-recover=all']
 for name,args in [('policy',['-I'+str(CP),str(CP/'tests/send_contract.cpp')]),('socket',['-I'+str(CP),'tests/learning/send_budget_socket.cpp',str(CP/'net/socket.cpp'),str(CP/'net/stream.cpp'),str(CP/'net/send_socket.cpp'),'-pthread']),('root-stream',['-I.','tests/learning/socket_stream.cpp','-Wl,--wrap=send,--wrap=recv'])]:
  exe=OUT/(name+'-sanitized');run([*flags,*args,'-o',str(exe)]);print(run([str(exe)],timeout=15).stdout,flush=True)
 run(['c++','-std=c++17','-Wall','-Wextra','-Wpedantic','-I.','tests/learning/socket_send_deadline.cpp','-Wl,--wrap=send','-pthread','-o',str(OUT/'root-deadline')]);print(run([str(OUT/'root-deadline')],timeout=10).stdout,flush=True)
 codec.OUT=OUT;codec.typed_echo()
 run([sys.executable,'scripts/check_learning_tcp_stream_windows.py'])
 print(run([sys.executable,'scripts/check_learning_send_windows.py']).stdout,flush=True)
 for build in [ROOT/'out/learning-checkpoints/83-sockets-check/root',ROOT/'out/learning-checkpoints/54-game-adapter-check/root']:
  run(['cmake','--build',str(build),'-j3']);r=run(['ctest','--test-dir',str(build),'--output-on-failure']);print(r.stdout[-300:],flush=True)
 print('Partial-send checks complete',flush=True)
if __name__=='__main__':main()
