"""Lesson 83: real loopback processes, lifetime, failure paths and cumulative builds."""
from pathlib import Path
import os,select,socket,subprocess
from check_learning_text_layout import run
ROOT=Path(__file__).resolve().parents[1]
SOURCE=ROOT/'docs/learn/checkpoints/83-sockets'
OUT=ROOT/'out/learning-checkpoints/83-sockets-check'
def process_cases(probe):
 def launch():
  p=subprocess.Popen([str(probe),'listen','0'],text=True,stdout=subprocess.PIPE,stderr=subprocess.PIPE)
  try:
   assert select.select([p.stdout],[],[],10)[0], 'listener readiness timeout'
   line=p.stdout.readline(); assert line.startswith('LISTEN 127.0.0.1:'),line
   return p,int(line.rsplit(':',1)[1])
  except BaseException:p.kill();p.communicate();raise
 def finish(p,code):
  try:
   out,err=p.communicate(timeout=10); assert p.returncode==code,(p.returncode,out,err)
   return out,err
  finally:
   if p.poll() is None:p.kill();p.communicate()
 p,port=launch()
 try:
  r=run([str(probe),'connect',str(port)]);assert r.stdout=='SENT 42 RECEIVED 42\n'
  assert finish(p,0)[0]=='RECEIVED 42 ECHOED 42\n'
 finally:
  if p.poll() is None:p.kill();p.communicate()
 p,port=launch()
 try:
  with socket.create_connection(('127.0.0.1',port),timeout=5) as client:client.shutdown(socket.SHUT_WR)
  assert 'error=0 eof=true' in finish(p,1)[1]
 finally:
  if p.poll() is None:p.kill();p.communicate()
 # A raw peer exercises every possible one-byte value, not just the literal42.
 for value in (0,128,255):
  p,port=launch()
  try:
   with socket.create_connection(('127.0.0.1',port),timeout=5) as client:
    client.sendall(bytes([value]));assert client.recv(1)==bytes([value])
   assert finish(p,0)[0]==f'RECEIVED {value} ECHOED {value}\n'
  finally:
   if p.poll() is None:p.kill();p.communicate()
 for args in [[],['listen','-1'],['listen','65536'],['listen','2x'],['listen','+1'],['listen',' 1'],['listen',''],['connect','0'],['unknown','1']]:
  r=subprocess.run([str(probe),*args],text=True,capture_output=True,timeout=5);assert r.returncode==2,(args,r)
 # Hold a bound but non-listening local socket so the chosen refusal port cannot race with another listener.
 with socket.socket() as reserved:
  reserved.bind(('127.0.0.1',0));port=reserved.getsockname()[1]
  r=subprocess.run([str(probe),'connect',str(port)],text=True,capture_output=True,timeout=5)
  assert r.returncode==1 and 'FAIL connect error=' in r.stderr
 print('Two-process echo, raw-byte range, EOF, refused connection and strict CLI passed.',flush=True)
def main():
 OUT.mkdir(parents=True,exist_ok=True)
 prev=SOURCE.parent/'82-audio-failure'
 for p in prev.rglob('*'):
  if p.is_file() and p.relative_to(prev).as_posix() not in {'CMakeLists.txt','README.md','DESIGN.md'}:
   assert p.read_bytes()==(SOURCE/p.relative_to(prev)).read_bytes(),p
 env={**os.environ,'SDL_VIDEODRIVER':'dummy','SDL_AUDIODRIVER':'dummy','DISPLAY':'','WAYLAND_DISPLAY':''}
 for backend in ['SCRIPTED','SDL']:
  build=OUT/backend.lower();run(['cmake','-S',str(SOURCE),'-B',str(build),'-DSTUDY_PLATFORM='+backend,'-DCMAKE_BUILD_TYPE=Release','-DCMAKE_CXX_FLAGS=-Wall -Wextra -Wpedantic'])
  r=run(['cmake','--build',str(build),'-j3']);(OUT/f'build-{backend}.log').write_text(r.stdout+r.stderr);assert 'warning:' not in r.stdout+r.stderr
  r=run(['ctest','--test-dir',str(build),'--output-on-failure'],env=env);(OUT/f'ctest-{backend}.log').write_text(r.stdout);print(r.stdout[-300:],flush=True)
 process_cases(OUT/'scripted/socket_probe')
 binary=OUT/'socket-sanitized'
 run(['c++','-std=c++17','-Wall','-Wextra','-Wpedantic','-fsanitize=address,undefined','-fno-sanitize-recover=all','-I'+str(SOURCE),str(SOURCE/'net/socket.cpp'),str(SOURCE/'tests/socket_contract.cpp'),'-o',str(binary)])
 print(run([str(binary)]).stdout,flush=True)
 binary=OUT/'root-ownership'
 run(['c++','-std=c++17','-Wall','-Wextra','-Wpedantic','-I'+str(ROOT),str(ROOT/'tests/learning/socket_ownership.cpp'),'-Wl,--wrap=close,--wrap=setsockopt,--wrap=fcntl','-o',str(binary)])
 print(run([str(binary)]).stdout,flush=True)
 # Public Windows ABI representation without an SDK. This is not native execution.
 check=OUT/'windows-handle.cpp';check.write_text('''#include "net/socket.h"
#include "net/reactor.h"
#include <type_traits>
static_assert(sizeof(net::NativeSocket)==sizeof(void*));
static_assert(net::socket_valid(0));
static_assert(net::socket_valid(net::NativeSocket{1}<<40));
static_assert(!net::socket_valid(net::kInvalidSocket));
static_assert(std::is_same_v<decltype(net::TcpSocket{}.fd()),net::NativeSocket>);
static_assert(std::is_same_v<decltype(&net::Reactor::add),bool (net::Reactor::*)(net::NativeSocket,unsigned,void*)>);
int main() {}
''')
 run(['c++','-std=c++17','-D_WIN32','-I'+str(ROOT),str(check),'-o',str(OUT/'windows-handle')])
 run([str(OUT/'windows-handle')])
 build=OUT/'root'
 run(['cmake','-S',str(ROOT),'-B',str(build),'-DTETRIS_BUILD_GAME=OFF','-DTETRIS_BUILD_TEST=ON','-DTETRIS_BUILD_RELAY=ON','-DTETRIS_BUILD_REACTOR=ON','-DCMAKE_BUILD_TYPE=Release'])
 run(['cmake','--build',str(build),'-j3'])
 r=run(['ctest','--test-dir',str(build),'--output-on-failure'],env=env);(OUT/'root-ctest.log').write_text(r.stdout);print(r.stdout[-300:],flush=True)
 assert run([str(build/'sim_hash_dump')]).stdout==(ROOT/'python/tests/_sim_hash_dump.txt').read_text()
 print('Socket checkpoint and root regression checks passed; Windows/macOS native execution not covered.',flush=True)
if __name__=='__main__':main()
