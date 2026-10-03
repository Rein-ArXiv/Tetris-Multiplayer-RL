"""Lesson89: seed codec, host/peer handshake, real Session role and payload policy."""
from pathlib import Path
import os,sys,socket,subprocess,re,time
from check_learning_text_layout import run
from check_learning_framing import exact,finish,line,frame
ROOT=Path(__file__).resolve().parents[1]
CP=ROOT/'docs/learn/checkpoints/89-seed-handshake'
OUT=ROOT/'out/learning-checkpoints/89-seed-handshake-check'
sys.path.insert(0,str(ROOT/'python'));from netbot import framing as wire
HELLO=bytes([1,0,1,0,0,0])
def config(seed=77,delay=2,role=2):return HELLO+seed.to_bytes(8,'little')+(120).to_bytes(4,'little')+bytes([delay,role])
def readframe(c):
 n=int.from_bytes(exact(c,2),'little');b=exact(c,n);return b[0],b[1:]
def launch_host(seed=77):
 p=subprocess.Popen([str(OUT/'scripted/seed_probe'),'listen','0',str(seed)],stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True)
 ready=line(p);assert ready.startswith('LISTEN ');return p,int(ready.split()[1])
def cleanup(p):
 if p.poll() is None:p.kill();p.communicate()
def exchanges():
 for seed in [0,1,77,2**64-1]:
  host,port=launch_host(seed)
  try:
   peer=run([str(OUT/'scripted/seed_probe'),'connect',str(port)],timeout=10)
   hs=finish(host,0)[0];a=re.search(r'seed=(\d+) countdown=(\d+) delay=(\d+) hash=(\d+)',hs);b=re.search(r'seed=(\d+) countdown=(\d+) delay=(\d+) hash=(\d+)',peer.stdout)
   assert a and b and a.groups()==b.groups() and int(a[1])==seed
  finally:cleanup(host)
 # Host: wrong/missing HELLO and wrong/mismatching/missing ACK.
 cases=[('hello',frame(HELLO,12)),('hello',frame(b'\2'+HELLO[1:],10)),('hello',b''),('ack',frame(config(78),12)),('ack',frame(config(),11)),('ack',frame(config()+b'\0',12)),('ack',b'')]
 for phase,data in cases:
  p,port=launch_host()
  try:
   with socket.create_connection(('127.0.0.1',port),timeout=5) as c:
    c.settimeout(5)
    if phase=='ack':c.sendall(frame(HELLO,10));assert readframe(c)==(11,config())
    c.sendall(data);c.shutdown(socket.SHUT_WR)
    out=finish(p,1)[0];assert 'unchanged=1 active=0' in out
  finally:cleanup(p)
 # Peer: unsupported version/rules, wrong role/type/size/range and EOF.
 invalid=[frame(config(),12),frame(config(role=1),11),frame(config(delay=31),11),frame(config()[:-1],11),frame(b'\2'+config()[1:],11),frame(config()[:2]+b'\2'+config()[3:],11),b'']
 for reply in invalid:
  with socket.socket() as l:
   l.bind(('127.0.0.1',0));l.listen();l.settimeout(5)
   p=subprocess.Popen([str(OUT/'scripted/seed_probe'),'connect',str(l.getsockname()[1])],stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True)
   try:
    c,_=l.accept()
    with c:
     c.settimeout(5);assert readframe(c)==(10,HELLO);c.sendall(reply);c.shutdown(socket.SHUT_WR)
     out=finish(p,1)[0];assert 'unchanged=1 active=0' in out
   finally:cleanup(p)
 print('Handshake: four seeds produce identical initial hashes; seven host and seven peer invalid/missing exchanges preserve output and close',flush=True)
def root_sessions():
 def seed(n=77,role=2):return n.to_bytes(8,'little')+(120).to_bytes(4,'little')+bytes([2,role])
 def send(c,kind,p):c.sendall(wire.build_frame(kind,p))
 def barrier(c,t):send(c,wire.MsgType.HASH,t.to_bytes(4,'little')+b'\0'*8)
 def receive(c,kind):
  for _ in range(1200):
   n=int.from_bytes(exact(c,2),'little');b=exact(c,n+4)
   if b[0]==int(kind):return b[1:n]
  raise AssertionError('message missing')
 for mode in ['host','client']:
  with socket.socket() as l:
   l.bind(('127.0.0.1',0));port=l.getsockname()[1]
   if mode=='host':l.close() # Session API accepts a requested port, not an existing listener.
   else:l.listen();l.settimeout(6)
   p=subprocess.Popen([str(OUT/'session-seed'),mode,str(port)],stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True)
   try:
    if mode=='host':
     end=time.monotonic()+5
     while True:
      try:c=socket.create_connection(('127.0.0.1',port),timeout=1);break
      except ConnectionRefusedError:
       if time.monotonic()>=end:raise
       time.sleep(.01)
    else:c,_=l.accept()
    with c:
     c.settimeout(6)
     if mode=='host':
      assert receive(c,wire.MsgType.SEED)==seed();barrier(c,600)
      assert receive(c,wire.MsgType.SEED)==seed(78);barrier(c,1200)
     else:
      send(c,wire.MsgType.SEED,seed());barrier(c,600);assert int.from_bytes(receive(c,wire.MsgType.HASH)[:4],'little')==1200
      for bad in [seed(88,3),seed(89)[:-1],seed(90)+b'\0']:send(c,wire.MsgType.SEED,bad)
      barrier(c,1800);assert int.from_bytes(receive(c,wire.MsgType.HASH)[:4],'little')==2400
      send(c,wire.MsgType.SEED,seed(79));barrier(c,3000)
     assert 'SEED_SESSION_OK' in finish(p,0)[0]
   finally:cleanup(p)
 print('Actual Session: initial/rematch receiver role is complementary; malformed SEED preserves params/ready; valid rematch accepted',flush=True)
def main():
 OUT.mkdir(parents=True,exist_ok=True)
 prev=CP.parent/'88-connection-lifetime'
 for p in prev.rglob('*'):
  if p.is_file() and p.relative_to(prev).as_posix() not in {'README.md','DESIGN.md','CMakeLists.txt','net/connection.h','net/connection.cpp'}:assert p.read_bytes()==(CP/p.relative_to(prev)).read_bytes(),p
 if '--skip-build' not in sys.argv:
  for backend in ['SCRIPTED','SDL']:
   b=OUT/backend.lower();run(['cmake','-S',str(CP),'-B',str(b),'-DSTUDY_PLATFORM='+backend,'-DCMAKE_BUILD_TYPE=Release','-DCMAKE_CXX_FLAGS=-Wall -Wextra -Wpedantic'])
   p=run(['cmake','--build',str(b),'-j3']);(OUT/f'build-{backend}.log').write_text(p.stdout+p.stderr);assert 'warning:' not in p.stdout+p.stderr,p.stderr
   p=run(['ctest','--test-dir',str(b),'--output-on-failure'],env={**os.environ,'SDL_VIDEODRIVER':'dummy','SDL_AUDIODRIVER':'dummy','DISPLAY':'','WAYLAND_DISPLAY':''});print(backend,p.stdout[-180:],flush=True)
 flags=['c++','-std=c++17','-Wall','-Wextra','-Wpedantic','-fsanitize=address,undefined','-fno-sanitize-recover=all']
 run([*flags,'-I'+str(CP),str(CP/'tests/seed_protocol_contract.cpp'),'-o',str(OUT/'codec-sanitized')]);print(run([str(OUT/'codec-sanitized')]).stdout,flush=True)
 run([*flags,'-I'+str(CP),str(CP/'tests/seed_lifetime_contract.cpp'),*[str(CP/'net'/f) for f in ['connection.cpp','socket.cpp','stream.cpp','send_socket.cpp']],'-pthread','-o',str(OUT/'lifetime-sanitized')]);print(run([str(OUT/'lifetime-sanitized')],timeout=10).stdout,flush=True)
 run([*flags,'-I.','tests/learning/session_seed.cpp','net/session.cpp','net/socket.cpp','net/framing.cpp','net/wss_client.cpp','-pthread','-o',str(OUT/'session-seed')])
 root_sessions();exchanges()
 for b in [ROOT/'out/learning-checkpoints/83-sockets-check/root',ROOT/'out/learning-checkpoints/54-game-adapter-check/root']:
  run(['cmake','--build',str(b),'-j3']);p=run(['ctest','--test-dir',str(b),'--output-on-failure']);print(p.stdout[-250:],flush=True)
 print('Seed handshake checks complete',flush=True)
if __name__=='__main__':main()
