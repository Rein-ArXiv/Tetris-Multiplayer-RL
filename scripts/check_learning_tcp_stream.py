"""Lesson84: byte-stream partitions, real processes and root partial-I/O regression."""
from pathlib import Path
import os,re,select,socket,subprocess
from check_learning_text_layout import run
from check_learning_sockets import process_cases as one_byte_cases
ROOT=Path(__file__).resolve().parents[1]
SOURCE=ROOT/'docs/learn/checkpoints/84-tcp-stream'
OUT=ROOT/'out/learning-checkpoints/84-tcp-stream-check'
def stream_cases(probe):
 def launch(cap):
  p=subprocess.Popen([str(probe),'listen','0',str(cap)],text=True,stdout=subprocess.PIPE,stderr=subprocess.PIPE)
  try:
   assert select.select([p.stdout],[],[],10)[0], 'listener readiness timeout'
   line=p.stdout.readline();assert line.startswith('LISTEN 127.0.0.1:'),line
   return p,int(line.rsplit(':',1)[1])
  except BaseException:p.kill();p.communicate();raise
 def cleanup(p):
  if p.poll() is None:p.kill();p.communicate()
 def finish(p,code):
  try:
   out,err=p.communicate(timeout=10);assert p.returncode==code,(p.returncode,out,err)
   return out,err
  finally:cleanup(p)
 for cap in [1,2,4,16]:
  for mode in ['one','two','bytes']:
   p,port=launch(cap)
   try:
    r=run([str(probe),'connect',str(port),mode])
    assert 'VERIFIED 6' in r.stdout
    out,err=finish(p,0);assert 'TOTAL count=6 hex=41 42 43 44 45 46' in out
    chunks=re.findall(r'^READ offset=(\d+) count=(\d+) hex=([0-9A-F ]+)$',out,re.M)
    offset=0;data=b''
    for start,count,hexes in chunks:
     assert int(start)==offset and 0<int(count)<=cap
     part=bytes.fromhex(hexes);assert len(part)==int(count)
     data+=part;offset+=int(count)
    assert data==b'ABCDEF'
    if cap==1:assert len(chunks)==6
   finally:cleanup(p)
 for payload in [b'',bytes(range(64)),b'\0\xffABC\0']:
  p,port=launch(16)
  try:
   with socket.create_connection(('127.0.0.1',port),timeout=5) as client:
    client.sendall(payload);client.shutdown(socket.SHUT_WR);got=b''
    while True:
     part=client.recv(32)
     if not part:break
     got+=part
    assert got==payload
   out,err=finish(p,0);assert f'TOTAL count={len(payload)}' in out
  finally:cleanup(p)
 p,port=launch(16)
 try:
  with socket.create_connection(('127.0.0.1',port),timeout=5) as client:
   client.sendall(bytes(range(65)));client.shutdown(socket.SHUT_WR)
   try:client.recv(32)
   except ConnectionResetError:pass
  assert 'reason=overflow capacity=64' in finish(p,1)[1]
 finally:cleanup(p)
 # A controlled server sends early EOF, wrong bytes or an oversized response.
 for payload,reason in [(b'ABCDE','length'),(b'ABCDEX','content'),(b'x'*65,'overflow')]:
  with socket.socket() as server:
   server.settimeout(5);server.bind(('127.0.0.1',0));server.listen(1)
   p=subprocess.Popen([str(probe),'connect',str(server.getsockname()[1]),'two'],text=True,stdout=subprocess.PIPE,stderr=subprocess.PIPE)
   try:
    peer,_=server.accept()
    with peer:
     peer.settimeout(5);request=b''
     while True:
      data=peer.recv(16)
      if not data:break
      request+=data
     assert request==b'ABCDEF'
     peer.sendall(payload);peer.shutdown(socket.SHUT_WR)
    assert f'reason={reason}' in finish(p,1)[1]
   finally:cleanup(p)
 for args in [[],['--help','extra'],['listen','0','0'],['listen','0','17'],['listen','0','2x'],['connect','0','one'],['connect','1','unknown'],['listen','65536','1'],['listen','-1','1']]:
  r=subprocess.run([str(probe),*args],text=True,capture_output=True,timeout=5);assert r.returncode==2,(args,r)
 print('12 mode/read-cap combinations, binary/empty/64/65-byte limits and malformed replies passed.',flush=True)
def main():
 OUT.mkdir(parents=True,exist_ok=True)
 prev=SOURCE.parent/'83-sockets'
 for p in prev.rglob('*'):
  if p.is_file() and p.relative_to(prev).as_posix() not in {'CMakeLists.txt','README.md','DESIGN.md'}:
   assert p.read_bytes()==(SOURCE/p.relative_to(prev)).read_bytes(),p
 env={**os.environ,'SDL_VIDEODRIVER':'dummy','SDL_AUDIODRIVER':'dummy','DISPLAY':'','WAYLAND_DISPLAY':''}
 for backend in ['SCRIPTED','SDL']:
  build=OUT/backend.lower()
  run(['cmake','-S',str(SOURCE),'-B',str(build),'-DSTUDY_PLATFORM='+backend,'-DCMAKE_BUILD_TYPE=Release','-DCMAKE_CXX_FLAGS=-Wall -Wextra -Wpedantic'])
  r=run(['cmake','--build',str(build),'-j3']);(OUT/f'build-{backend}.log').write_text(r.stdout+r.stderr);assert 'warning:' not in r.stdout+r.stderr
  r=run(['ctest','--test-dir',str(build),'--output-on-failure'],env=env);(OUT/f'ctest-{backend}.log').write_text(r.stdout);print(r.stdout[-300:],flush=True)
 one_byte_cases(OUT/'scripted/socket_probe')
 stream_cases(OUT/'scripted/stream_probe')
 flags=['c++','-std=c++17','-Wall','-Wextra','-Wpedantic','-fsanitize=address,undefined','-fno-sanitize-recover=all']
 binary=OUT/'stream-sanitized'
 run([*flags,'-I'+str(SOURCE),str(SOURCE/'net/socket.cpp'),str(SOURCE/'net/stream.cpp'),str(SOURCE/'tests/stream_contract.cpp'),'-o',str(binary)])
 print(run([str(binary)]).stdout,flush=True)
 binary=OUT/'root-stream'
 run([*flags,'-I'+str(ROOT),str(ROOT/'tests/learning/socket_stream.cpp'),'-Wl,--wrap=send,--wrap=recv','-o',str(binary)])
 print(run([str(binary)]).stdout,flush=True)
 for build in [ROOT/'out/learning-checkpoints/83-sockets-check/root',ROOT/'out/learning-checkpoints/54-game-adapter-check/root']:
  run(['cmake','--build',str(build),'-j3'])
  r=run(['ctest','--test-dir',str(build),'--output-on-failure'],env=env);(OUT/(build.parent.name+'-ctest.log')).write_text(r.stdout);print(r.stdout[-300:],flush=True)
  assert run([str(build/'sim_hash_dump')]).stdout==(ROOT/'python/tests/_sim_hash_dump.txt').read_text()
 run(['python3',str(ROOT/'scripts/check_learning_tcp_stream_windows.py')])
 print('Stream checkpoint and root I/O checks passed; Windows/macOS native execution not covered.',flush=True)
if __name__=='__main__':main()
