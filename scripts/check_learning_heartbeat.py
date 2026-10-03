"""Lesson94: heartbeat time policy, worker control path and root response correlation."""
from pathlib import Path
import os,sys,subprocess,socket,time,json
from check_learning_text_layout import run
from check_learning_framing import finish,line,frame
from check_learning_seed import cleanup,readframe
from check_part_docs import normalized
ROOT=Path(__file__).resolve().parents[1];CP=ROOT/'docs/learn/checkpoints/94-heartbeat';OUT=ROOT/'out/learning-checkpoints/94-heartbeat-check'
def root_probe(before=False):
 src=(ROOT/('out/learning-jobs/094-before-session.cpp' if before else 'net/session.cpp')).read_text()
 a=src.index('    case MsgType::PING: {');b=src.index('    case MsgType::CHAT:',a)
 (OUT/'pong_handler.inc').write_text(src[a:b])
 exe=OUT/('root-before' if before else 'root-after')
 run(['c++','-std=c++17','-Wall','-Wextra','-Wpedantic','-fsanitize=address,undefined','-fno-sanitize-recover=all','-I'+str(ROOT),'-I'+str(OUT),str(ROOT/'tests/learning/pong_handler.cpp'),str(ROOT/'net/framing.cpp'),'-o',str(exe)])
 p=subprocess.run([str(exe)],text=True,capture_output=True);assert p.returncode==(1 if before else 0),p.stderr
 print('Root handler:', 'before fails empty-PONG assertion' if before else p.stdout.strip(),flush=True)
 if before:assert 'lastPongMs==0' in p.stderr
 else:
  assert 'pendingPongs_.remember(static_cast<uint64_t>(now)))' in src
  assert 'pendingPongs_ = PongWindow{};' in src

def exchanges():
 def launch():
  p=subprocess.Popen([str(OUT/'scripted/heartbeat_probe'),'listen','0'],stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True)
  port=int(line(p).split()[1]);return p,port
 for _ in range(3):
  p,port=launch()
  try:
   peer=run([str(OUT/'scripted/heartbeat_probe'),'connect',str(port)],timeout=5)
   host=finish(p,0)[0];assert 'END 0 confirmations=' in host and 'done=1' in host
   assert 'END 0 confirmations=' in peer.stdout and 'done=1' in peer.stdout
  finally:cleanup(p)
 # A valid PING stream without matching PONG must not renew our lease.
 for fault in ['silent','wrong','replay','malformed','ping_only']:
  p,port=launch()
  try:
   with socket.create_connection(('127.0.0.1',port),timeout=5) as c:
    c.settimeout(2);kind,token=readframe(c);assert kind==30;c.settimeout(0.05)
    if fault=='replay':c.sendall(frame(token,31))
    if fault=='malformed':c.sendall(frame(token[:-1],31))
    start=time.monotonic()
    while p.poll() is None and time.monotonic()-start<2:
     try:
      if fault=='wrong':c.sendall(frame((9999).to_bytes(8,'little'),31))
      elif fault=='replay':c.sendall(frame(token,31))
      elif fault=='ping_only':c.sendall(frame((7).to_bytes(8,'little'),30))
      try:c.recv(512)
      except socket.timeout:pass
     except (BrokenPipeError,ConnectionResetError):break
     time.sleep(0.01)
    output=finish(p,1)[0]
    assert ('END 3 ' if fault=='malformed' else 'END 7 ') in output,(fault,output)
  finally:cleanup(p)
 print('Three real peer pairs; silence/wrong/replayed/malformed/PING-only peer failures passed',flush=True)
def main():
 OUT.mkdir(parents=True,exist_ok=True)
 prev=CP.parent/'93-thread-queues'
 allowed={'README.md','DESIGN.md','CMakeLists.txt','net/thread_link.h','net/thread_link.cpp'}
 for p in prev.rglob('*'):
  if p.is_file() and p.relative_to(prev).as_posix() not in allowed:assert p.read_bytes()==(CP/p.relative_to(prev)).read_bytes(),p
 if '--skip-build' not in sys.argv:
  for backend in ['SCRIPTED','SDL']:
   b=OUT/backend.lower();run(['cmake','-S',str(CP),'-B',str(b),'-DSTUDY_PLATFORM='+backend,'-DCMAKE_BUILD_TYPE=Release','-DCMAKE_CXX_FLAGS=-Wall -Wextra -Wpedantic'])
   p=run(['cmake','--build',str(b),'-j3']);(OUT/f'build-{backend}.log').write_text(p.stdout+p.stderr);assert 'warning:' not in p.stdout+p.stderr,p.stderr
   p=run(['ctest','--test-dir',str(b),'--output-on-failure'],env={**os.environ,'SDL_VIDEODRIVER':'dummy','SDL_AUDIODRIVER':'dummy','DISPLAY':'','WAYLAND_DISPLAY':''});print(backend,p.stdout[-240:],flush=True)
 sources=[CP/'tests/heartbeat_contract.cpp',*[CP/f'net/{f}.cpp' for f in ['socket','stream','send_socket','receive_socket','thread_link']]]
 flags=['c++','-std=c++17','-Wall','-Wextra','-Wpedantic','-pthread','-fsanitize=address,undefined','-fno-sanitize-recover=all','-I'+str(CP)]
 run([*flags,*map(str,sources),'-o',str(OUT/'beat-sanitized')]);print(run([str(OUT/'beat-sanitized')]).stdout,flush=True)
 root_probe(True);root_probe();exchanges()
 lesson=ROOT/'docs/learn/lessons/094.json'
 if lesson.exists():
  corpora={lang:'\n'.join(p.read_text() for p in CP.rglob('*') if p.is_file() and (p.suffix in ['.cpp','.h'] if lang=='cpp' else p.name=='CMakeLists.txt')) for lang in ['cpp','cmake']}
  count=0
  for section in json.loads(lesson.read_text())['sections']:
   for code in section.get('codes',[]):
    if 'file' not in code and code['language'] in corpora:
     assert normalized(code['text'],code['language']) in normalized(corpora[code['language']],code['language']),code['label'];count+=1
  print('Inline snippets',count,flush=True)
 for b in [ROOT/'out/learning-checkpoints/83-sockets-check/root',ROOT/'out/learning-checkpoints/54-game-adapter-check/root']:
  run(['cmake','--build',str(b),'-j3']);p=run(['ctest','--test-dir',str(b),'--output-on-failure']);print(p.stdout[-220:],flush=True)
 print('Heartbeat checks complete',flush=True)
if __name__=='__main__':main()
