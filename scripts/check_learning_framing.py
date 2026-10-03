"""Lesson85: bounded framing, real Session failure paths and C++/Python wire comparison."""
from pathlib import Path
import os,sys,select,socket,subprocess
from check_learning_text_layout import run
ROOT=Path(__file__).resolve().parents[1]
CP=ROOT/'docs/learn/checkpoints/85-framing'
OUT=ROOT/'out/learning-checkpoints/85-framing-check'
sys.path.insert(0,str(ROOT/'python'))
from netbot import framing as wire

def finish(p,code):
 try:
  out,err=p.communicate(timeout=10);assert p.returncode==code,(p.returncode,out,err)
  return out,err
 finally:
  if p.poll() is None:p.kill();p.communicate()
def line(p):
 assert select.select([p.stdout],[],[],7)[0], 'readiness timeout'
 return p.stdout.readline().strip()
def frame(payload=b'',kind=1):return (len(payload)+1).to_bytes(2,'little')+bytes([kind])+payload
def exact(sock,n):
 data=b''
 while len(data)<n:
  part=sock.recv(n-len(data));assert part,'early EOF';data+=part
 return data

def processes():
 probe=OUT/'scripted/framing_probe'
 def launch(cap):
  p=subprocess.Popen([str(probe),'listen','0',str(cap)],text=True,stdout=subprocess.PIPE,stderr=subprocess.PIPE)
  s=line(p);assert s.startswith('LISTEN '),s
  return p,int(s.split()[1])
 for cap in [1,2,4,16]:
  for chunk in [1,2,3,16]:
   p,port=launch(cap)
   try:
    assert 'VERIFIED 3 FRAMES' in run([str(probe),'connect',str(port),str(chunk)],timeout=10).stdout
    out,err=finish(p,0);assert 'FRAMES 3' in out
   finally:
    if p.poll() is None:p.kill();p.communicate()
 # Each reply must arrive before the peer closes its write direction.
 p,port=launch(16)
 try:
  with socket.create_connection(('127.0.0.1',port),timeout=5) as peer:
   for payload in [b'',bytes(range(32)),b'\x00\xff']:
    peer.sendall(frame(payload));assert exact(peer,len(payload)+3)==frame(payload,2)
   peer.shutdown(socket.SHUT_WR);assert peer.recv(1)==b''
  assert 'FRAMES 3' in finish(p,0)[0]
 finally:
  if p.poll() is None:p.kill();p.communicate()
 for data,reason,shutdown in [(b'\0\0','malformed_length',False),(b'\x22\0','malformed_length',False),(b'\x04','truncated_frame',True),(b'\x04\0\1A','truncated_frame',True),(frame(b'',99),'unknown_type',False)]:
  p,port=launch(16)
  try:
   with socket.create_connection(('127.0.0.1',port),timeout=5) as peer:
    peer.sendall(data)
    if shutdown:peer.shutdown(socket.SHUT_WR)
    assert f'reason={reason}' in finish(p,1)[1]
  finally:
   if p.poll() is None:p.kill();p.communicate()
 replies=frame(b'ABC',2)+frame(b'',2)+frame(b'\0\xff*',2)
 for data,reason in [(b'','missing_replies'),(frame(b'BAD',2),'payload_mismatch'),(frame(b'ABC',1),'unexpected_type'),(b'\0\0','malformed_length'),(replies+frame(b'',2),'extra_frame'),(replies+b'\1','truncated_tail')]:
  with socket.socket() as listener:
   listener.settimeout(5);listener.bind(('127.0.0.1',0));listener.listen(1)
   p=subprocess.Popen([str(probe),'connect',str(listener.getsockname()[1]),'2'],text=True,stdout=subprocess.PIPE,stderr=subprocess.PIPE)
   try:
    peer,_=listener.accept()
    with peer:
     peer.settimeout(5);assert exact(peer,15)==frame(b'ABC')+frame()+frame(b'\0\xff*')
     peer.sendall(data);peer.shutdown(socket.SHUT_WR)
     assert f'reason={reason}' in finish(p,1)[1]
   finally:
    if p.poll() is None:p.kill();p.communicate()
 for args in [[],['--help','extra'],['listen','0','0'],['listen','0','17'],['listen','0','2x'],['connect','0','1'],['connect','1','-1'],['listen','65536','1'],['listen','-1','1']]:
  r=subprocess.run([str(probe),*args],text=True,capture_output=True,timeout=5);assert r.returncode==2,(args,r)
 print('16 real-process combinations; replies before EOF; maximum/binary/empty, malformed requests/replies and CLI passed',flush=True)

def session_cases():
 for mode in ['io','room','queue','lobby']:
  with socket.socket() as listener:
   listener.settimeout(5);listener.bind(('127.0.0.1',0));listener.listen(1)
   p=subprocess.Popen([str(OUT/'session-framing'),mode,str(listener.getsockname()[1])],text=True,stdout=subprocess.PIPE,stderr=subprocess.PIPE)
   try:
    peer,_=listener.accept()
    with peer:
     peer.settimeout(5)
     # Observe initial HELLO / ROOM_CREATE / QUEUE_JOIN before sending failure.
     length=int.from_bytes(exact(peer,2),'little');exact(peer,length+4)
     if mode=='lobby':
      peer.sendall(wire.build_frame(wire.MsgType.MATCH_FOUND,b'\1'+bytes(8)))
      assert line(p)=='MATCHED'
     peer.sendall(b'\x02\x10') # LEN4098: two bytes suffice; peer stays open.
     assert 'FAILED_CLOSED' in finish(p,0)[0]
   finally:
    if p.poll() is None:p.kill();p.communicate()
 print('Actual Session io/room/queue/lobby reject oversized header while peer remains open',flush=True)

def parity():
 def hx(b):return b.hex() or '-'
 commands=[];expected=[]
 for kind in wire.MsgType:
  for payload in [b'',b'\0\xffABC',bytes(range(256))*16]:
   commands.append(f'B {int(kind)} {hx(payload)}');expected.append(hx(wire.build_frame(kind,payload)))
 a=wire.build_frame(wire.MsgType.HELLO,b'');b=wire.build_frame(wire.MsgType.INPUT,b'\0\xff');data=a+b
 sequences=[]
 # Exhaust all 32768 partitions across the same 16-byte pair.
 for cuts in range(1<<(len(data)-1)):
  begin=0;chunks=[]
  for end in range(1,len(data)+1):
   if end==len(data) or cuts&(1<<(end-1)):chunks.append(data[begin:end]);begin=end
  sequences.append(chunks)
 corrupted=bytearray(b);corrupted[-1]^=1
 for sample in [a+b'\2\x10',b'\0'*6+a,bytes(corrupted)+a,b[:4],b'\xff\xff']:
  for split in range(len(sample)+1):sequences.append([sample[:split],sample[split:]])
 for chunks in sequences:
  commands.append('P '+' '.join(map(hx,chunks)));pending=bytearray();results=[]
  for chunk in chunks:
   pending+=chunk;ok=True
   try:frames=wire.parse_frames(pending)
   except wire.FramingError as e:ok=False;frames=e.frames
   results.append(('T' if ok else 'F')+':'+hx(pending)+''.join(f':{int(t)},{hx(p)}' for t,p in frames))
   if not ok:break
  expected.append(' '.join(results))
 r=run([str(OUT/'parity-probe')],input='\n'.join(commands)+'\n');actual=[x.strip() for x in r.stdout.splitlines()]
 assert actual==expected,next(((commands[i],actual[i],expected[i]) for i in range(min(len(actual),len(expected))) if actual[i]!=expected[i]),(len(actual),len(expected)))
 # Unknown TYPE is intentionally exposed by C++ and dropped by Python.
 unknown=bytearray(wire.build_frame(wire.MsgType.HELLO,b''));unknown[2]=255
 assert wire.parse_frames(unknown.copy())==[]
 assert run([str(OUT/'parity-probe')],input='P '+hx(unknown)+'\n').stdout.strip()=='T:-:255,-'
 print(f'{len(commands)} direct C++/Python comparisons; unknown-type policy difference explicitly checked',flush=True)

def main():
 OUT.mkdir(parents=True,exist_ok=True)
 for p in (CP.parent/'84-tcp-stream').rglob('*'):
  if p.is_file() and p.relative_to(CP.parent/'84-tcp-stream').as_posix() not in {'README.md','DESIGN.md','CMakeLists.txt','net/byte_buffer.h'}:
   assert p.read_bytes()==(CP/p.relative_to(CP.parent/'84-tcp-stream')).read_bytes(),p
 if '--skip-build' not in sys.argv:
  for backend in ['SCRIPTED','SDL']:
   build=OUT/backend.lower();run(['cmake','-S',str(CP),'-B',str(build),'-DSTUDY_PLATFORM='+backend,'-DCMAKE_BUILD_TYPE=Release','-DCMAKE_CXX_FLAGS=-Wall -Wextra -Wpedantic'])
   r=run(['cmake','--build',str(build),'-j3']);(OUT/f'build-{backend}.log').write_text(r.stdout+r.stderr);assert 'warning:' not in r.stdout+r.stderr
   r=run(['ctest','--test-dir',str(build),'--output-on-failure'],env={**os.environ,'SDL_VIDEODRIVER':'dummy','SDL_AUDIODRIVER':'dummy','DISPLAY':'','WAYLAND_DISPLAY':''});(OUT/f'ctest-{backend}.log').write_text(r.stdout)
 flags=['c++','-std=c++17','-Wall','-Wextra','-Wpedantic']
 run([*flags,'-I.', 'tests/learning/session_framing.cpp','net/session.cpp','net/socket.cpp','net/framing.cpp','net/wss_client.cpp','-pthread','-o',str(OUT/'session-framing')])
 run([*flags,'-I.','tests/learning/framing_parity_probe.cpp','net/framing.cpp','-o',str(OUT/'parity-probe')])
 for name,args in [('contract',['-I'+str(CP),str(CP/'tests/framing_contract.cpp')]),('root',['-I.','tests/framing_test.cpp','net/framing.cpp'])]:
  binary=OUT/(name+'-sanitized');run([*flags,'-fsanitize=address,undefined','-fno-sanitize-recover=all',*args,'-o',str(binary)]);print(run([str(binary)]).stdout,flush=True)
 processes();session_cases();parity()
 run([str(ROOT/'.venv/bin/python'),'-m','pytest','python/tests/test_framing_parity.py','-q'],env={**os.environ,'PYTHONPATH':str(ROOT/'python')})
 for build in [ROOT/'out/learning-checkpoints/83-sockets-check/root',ROOT/'out/learning-checkpoints/54-game-adapter-check/root']:
  run(['cmake','--build',str(build),'-j3']);r=run(['ctest','--test-dir',str(build),'--output-on-failure']);print(r.stdout[-300:],flush=True)
 print('Framing checks complete',flush=True)
if __name__=='__main__':main()
