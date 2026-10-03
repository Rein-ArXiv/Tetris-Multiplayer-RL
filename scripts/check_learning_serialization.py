"""Lesson86: field extents, typed echo, and actual Session payload validation."""
from pathlib import Path
import os,sys,subprocess,socket,select
from check_learning_text_layout import run
from check_learning_framing import exact,finish,line,frame
ROOT=Path(__file__).resolve().parents[1]
CP=ROOT/'docs/learn/checkpoints/86-serialization'
OUT=ROOT/'out/learning-checkpoints/86-serialization-check'
sys.path.insert(0,str(ROOT/'python'))
from netbot import framing as wire

def root_sessions():
 for mode in ['good','wrap','unknown','trailing','zero','result']:
  with socket.socket() as listener:
   listener.settimeout(5);listener.bind(('127.0.0.1',0));listener.listen(1)
   p=subprocess.Popen([str(OUT/'session-payload'),mode,str(listener.getsockname()[1])],text=True,stdout=subprocess.PIPE,stderr=subprocess.PIPE)
   try:
    peer,_=listener.accept()
    with peer:
     peer.settimeout(5);size=int.from_bytes(exact(peer,2),'little');exact(peer,size+4)
     data={'good':b'\0'*4+b'\2\0\3\x10',
           'wrap':b'\xfe\xff\xff\xff\4\0\x10\x10\x10\x10',
           'unknown':b'\0'*4+b'\2\0\1\x20',
           'trailing':b'\0'*4+b'\1\0\x10\0',
           'zero':b'\0'*6,
           'result':b'\0\0\0\x80\xff\xff\xff\x7f\xff\xff\xff\xff'}[mode]
     kind=wire.MsgType.MATCH_RESULT if mode=='result' else wire.MsgType.INPUT
     barrier=wire.build_frame(wire.MsgType.HASH,(600).to_bytes(4,'little')+(456).to_bytes(8,'little'))
     peer.sendall(wire.build_frame(kind,data)+barrier)
     assert 'PAYLOAD_POLICY_OK' in finish(p,0)[0]
   finally:
    if p.poll() is None:p.kill();p.communicate()
 print('Real Session: known masks accepted; wrapped tick, late invalid bit, extra/zero length rejected; signed result endpoints decoded',flush=True)

def typed_echo():
 client=OUT/'scripted/serialization_probe';server=OUT/'scripted/framing_probe'
 for cap in [1,2,4,16]:
  p=subprocess.Popen([str(server),'listen','0',str(cap)],text=True,stdout=subprocess.PIPE,stderr=subprocess.PIPE)
  try:
   ready=line(p);assert ready.startswith('LISTEN ')
   r=run([str(client),ready.split()[1]],timeout=10)
   assert 'PAYLOAD 04 03 02 01 03 00 01 00 10' in r.stdout and 'VERIFIED first_tick=16909060 count=3 masks=01,00,10' in r.stdout
   assert 'FRAMES 1' in finish(p,0)[0]
  finally:
   if p.poll() is None:p.kill();p.communicate()
 payload=b'\4\3\2\1\3\0\1\0\x10'
 cases=[(frame(payload[:-1],2),'grammar'),(frame(payload+b'\0',2),'grammar'),(frame(payload[:-1]+b'\x20',2),'grammar'),(frame(b'\xff'*4+b'\3\0\1\0\x10',2),'grammar'),(frame(payload,1),'type/count'),(frame(payload[:-1]+b'\1',2),'values'),(frame(payload,2)+b'\1','incomplete')]
 for data,reason in cases:
  with socket.socket() as listener:
   listener.settimeout(5);listener.bind(('127.0.0.1',0));listener.listen(1)
   p=subprocess.Popen([str(client),str(listener.getsockname()[1])],text=True,stdout=subprocess.PIPE,stderr=subprocess.PIPE)
   try:
    peer,_=listener.accept()
    with peer:
     peer.settimeout(5);assert exact(peer,12)==frame(payload)
     peer.sendall(data);peer.shutdown(socket.SHUT_WR)
     assert reason in finish(p,1)[1]
   finally:
    if p.poll() is None:p.kill();p.communicate()
 for args in [[],['--help','x'],['0'],['65536'],['-1'],['1x']]:
  r=subprocess.run([str(client),*args],capture_output=True,timeout=5);assert r.returncode==2
 print('Typed echo: four read caps, seven malformed replies and strict CLI passed',flush=True)

def main():
 OUT.mkdir(parents=True,exist_ok=True)
 prev=CP.parent/'85-framing'
 for p in prev.rglob('*'):
  if p.is_file() and p.relative_to(prev).as_posix() not in {'CMakeLists.txt','README.md','DESIGN.md'}:assert p.read_bytes()==(CP/p.relative_to(prev)).read_bytes(),p
 if '--skip-build' not in sys.argv:
  for backend in ['SCRIPTED','SDL']:
   build=OUT/backend.lower();run(['cmake','-S',str(CP),'-B',str(build),'-DSTUDY_PLATFORM='+backend,'-DCMAKE_BUILD_TYPE=Release','-DCMAKE_CXX_FLAGS=-Wall -Wextra -Wpedantic'])
   r=run(['cmake','--build',str(build),'-j3']);(OUT/f'build-{backend}.log').write_text(r.stdout+r.stderr);assert 'warning:' not in r.stdout+r.stderr
   r=run(['ctest','--test-dir',str(build),'--output-on-failure'],env={**os.environ,'SDL_VIDEODRIVER':'dummy','SDL_AUDIODRIVER':'dummy','DISPLAY':'','WAYLAND_DISPLAY':''});(OUT/f'ctest-{backend}.log').write_text(r.stdout)
 flags=['c++','-std=c++17','-Wall','-Wextra','-Wpedantic']
 run([*flags,'-I.','tests/learning/session_payload.cpp','net/session.cpp','net/socket.cpp','net/framing.cpp','net/wss_client.cpp','-pthread','-o',str(OUT/'session-payload')])
 for name,args in [('codec',['-I'+str(CP),str(CP/'tests/serialization_contract.cpp')]),('root',['-I.','tests/input_message_test.cpp','net/framing.cpp'])]:
  exe=OUT/(name+'-sanitized');run([*flags,'-fsanitize=address,undefined','-fno-sanitize-recover=all',*args,'-o',str(exe)]);print(run([str(exe)]).stdout,flush=True)
 root_sessions();typed_echo()
 for build in [ROOT/'out/learning-checkpoints/83-sockets-check/root',ROOT/'out/learning-checkpoints/54-game-adapter-check/root']:
  run(['cmake','--build',str(build),'-j3']);r=run(['ctest','--test-dir',str(build),'--output-on-failure']);print(r.stdout[-300:],flush=True)
 print('Serialization checks complete',flush=True)
if __name__=='__main__':main()
