"""Lesson88: owned connection state, real half-close/restart, Session start guards."""
from pathlib import Path
import os,sys,subprocess,socket,threading
from check_learning_text_layout import run
from check_learning_framing import line,finish,exact,frame
ROOT=Path(__file__).resolve().parents[1]
CP=ROOT/'docs/learn/checkpoints/88-connection-lifetime'
OUT=ROOT/'out/learning-checkpoints/88-connection-lifetime-check'

def sessions():
 with socket.socket() as listener:
  listener.bind(('127.0.0.1',0));listener.listen();listener.settimeout(.2)
  stopping=threading.Event();workers=[]
  def drain(peer):
   with peer:
    peer.settimeout(8)
    try:
     while peer.recv(8192):pass
    except OSError:pass
  def accept():
   while not stopping.is_set():
    try:peer,_=listener.accept()
    except socket.timeout:continue
    t=threading.Thread(target=drain,args=(peer,));workers.append(t);t.start()
  acceptor=threading.Thread(target=accept);acceptor.start()
  try:
   for mode,failed in [(m,0) for m in range(5)]+[(m,1) for m in [2,3,4]]:
    r=run([str(OUT/'session-lifetime'),str(mode),str(listener.getsockname()[1]),str(failed)],timeout=12)
    assert 'SESSION_LIFETIME_OK' in r.stdout
  finally:
   stopping.set();acceptor.join()
   for t in workers:t.join()
 print('Actual Session: eight active/failed entry paths reject all five starts; repeated Close and reconnect passed',flush=True)

def echo():
 server=OUT/'scripted/framing_probe';client=OUT/'scripted/connection_probe'
 for cap in [1,2,4,16]:
  p=subprocess.Popen([str(server),'listen','0',str(cap)],text=True,stdout=subprocess.PIPE,stderr=subprocess.PIPE)
  try:
   ready=line(p);assert ready.startswith('LISTEN ')
   r=run([str(client),ready.split()[1]],timeout=10)
   assert 'VERIFIED tick=42 count=1 send_closed=1 peer_eof=1' in r.stdout and 'CLOSED active=0 pending=0' in r.stdout
   finish(p,0)
  finally:
   if p.poll() is None:p.kill();p.communicate()
 payload=(42).to_bytes(4,'little')+b'\1\0\1';good=frame(payload,2)
 for data in [b'',frame(payload,1),frame(payload[:-1]+b'\2',2),good+good,good+b'\1',b'\0\0']:
  with socket.socket() as listener:
   listener.settimeout(5);listener.bind(('127.0.0.1',0));listener.listen()
   p=subprocess.Popen([str(client),str(listener.getsockname()[1])],text=True,stdout=subprocess.PIPE,stderr=subprocess.PIPE)
   try:
    peer,_=listener.accept()
    with peer:
     peer.settimeout(5);assert exact(peer,10)==frame(payload);assert peer.recv(1)==b''
     peer.sendall(data);peer.shutdown(socket.SHUT_WR);finish(p,1)
   finally:
    if p.poll() is None:p.kill();p.communicate()
 print('Connection probe: four read caps, request half-close before reply, six bad/missing/extra responses rejected',flush=True)

def main():
 OUT.mkdir(parents=True,exist_ok=True)
 for p in (CP.parent/'87-partial-send').rglob('*'):
  if p.is_file() and p.name not in {'CMakeLists.txt','README.md','DESIGN.md'}:
   assert p.read_bytes()==(CP/p.relative_to(CP.parent/'87-partial-send')).read_bytes(),p
 if '--skip-build' not in sys.argv:
  for backend in ['SCRIPTED','SDL']:
   build=OUT/backend.lower();run(['cmake','-S',str(CP),'-B',str(build),'-DSTUDY_PLATFORM='+backend,'-DCMAKE_BUILD_TYPE=Release','-DCMAKE_CXX_FLAGS=-Wall -Wextra -Wpedantic'])
   r=run(['cmake','--build',str(build),'-j3']);(OUT/f'build-{backend}.log').write_text(r.stdout+r.stderr);assert 'warning:' not in r.stdout+r.stderr
   r=run(['ctest','--test-dir',str(build),'--output-on-failure'],env={**os.environ,'SDL_VIDEODRIVER':'dummy','SDL_AUDIODRIVER':'dummy','DISPLAY':'','WAYLAND_DISPLAY':''});(OUT/f'ctest-{backend}.log').write_text(r.stdout);print(backend,r.stdout[-180:],flush=True)
 flags=['c++','-std=c++17','-Wall','-Wextra','-Wpedantic','-fsanitize=address,undefined','-fno-sanitize-recover=all']
 run([*flags,'-I'+str(CP),str(CP/'tests/connection_contract.cpp'),*[str(CP/'net'/f) for f in ['connection.cpp','socket.cpp','stream.cpp','send_socket.cpp']],'-pthread','-o',str(OUT/'connection-sanitized')])
 print(run([str(OUT/'connection-sanitized')],timeout=15).stdout,flush=True)
 run([*flags,'-I.','tests/learning/session_lifetime.cpp','net/session.cpp','net/socket.cpp','net/framing.cpp','net/wss_client.cpp','-pthread','-o',str(OUT/'session-lifetime')])
 sessions();echo()
 for build in [ROOT/'out/learning-checkpoints/83-sockets-check/root',ROOT/'out/learning-checkpoints/54-game-adapter-check/root']:
  run(['cmake','--build',str(build),'-j3']);r=run(['ctest','--test-dir',str(build),'--output-on-failure']);print(r.stdout[-250:],flush=True)
 print('Connection lifetime checks complete',flush=True)
if __name__=='__main__':main()
