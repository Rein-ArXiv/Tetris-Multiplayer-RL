"""Lesson91: local capture delay, arrival gaps, finite tails and real main policy."""
from pathlib import Path
import os,sys,socket,subprocess,re
from check_learning_text_layout import run
from check_learning_framing import finish,line,frame
from check_learning_seed import readframe,HELLO,cleanup
from check_learning_lockstep import batch
ROOT=Path(__file__).resolve().parents[1]
CP=ROOT/'docs/learn/checkpoints/91-input-delay'
OUT=ROOT/'out/learning-checkpoints/91-input-delay-check'
def launch(seed=77,delay=2):
 p=subprocess.Popen([str(OUT/'scripted/delay_probe'),'listen','0',str(seed),str(delay)],stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True)
 text=line(p);assert text.startswith('LISTEN ');return p,int(text.split()[1])
def exchanges():
 for seed,delay in [(0,2),(1,0),(77,2),(2**64-1,30)]:
  p,port=launch(seed,delay)
  try:
   peer=run([str(OUT/'scripted/delay_probe'),'connect',str(port)],timeout=10);host=finish(p,0)[0]
   pattern=r'ticks=(\d+) left=(\d+) right=(\d+) delay=(\d+) pending=(\d+)'
   a=re.search(pattern,host);b=re.search(pattern,peer.stdout)
   assert a and b and a.groups()==b.groups() and int(a[1])==32-delay and int(a[4])==delay and int(a[5])==delay
   assert 'WAIT next=0' in host and 'WAIT next=0' in peer.stdout
  finally:cleanup(p)
 future=batch(16,[0]*16);early=batch(0,[0]*16)
 cases=[('hole',future,1,0),('duplicate',future+future+early,0,30),('conflict',future+batch(16,[1]*16),1,0),('range',batch(32,[0]),1,0),('empty',b'',1,0)]
 for name,data,code,tick in cases:
  p,port=launch()
  try:
   with socket.create_connection(('127.0.0.1',port),timeout=5) as c:
    c.settimeout(5);c.sendall(frame(HELLO,10));kind,config=readframe(c)
    assert kind==11 and config[-2]==2;c.sendall(frame(config,12));assert readframe(c)[0]==20;assert readframe(c)[0]==20
    c.sendall(data);c.shutdown(socket.SHUT_WR);output=finish(p,code)[0]
    assert (f'FAILED next={tick}' if code else 'ticks=30') in output,(name,output)
  finally:cleanup(p)
 print('Delay network: four seed/delay cases preserve ordered hashes and pending tails; five malformed/incomplete exchanges passed',flush=True)
def root_policy(source=None,expect_failure=False):
 text=(source or ROOT/'src/main.cpp').read_text()
 a=text.index('int64_t lastLocalSent =');b=text.index(';',text.index('int64_t safeTick',a))+1
 (OUT/'main_delay_policy.inc').write_text(text[a:b])
 a=text.index('while ((int64_t)simTick <= safeTick &&');b=text.index('gameRemote->Tick();',a)+len('gameRemote->Tick();')
 (OUT/'main_input_loop.inc').write_text(text[a:b])
 flags=['c++','-std=c++17','-Wall','-Wextra','-Wpedantic','-fsanitize=address,undefined','-fno-sanitize-recover=all']
 exe=OUT/('root-before' if expect_failure else 'root-after')
 run([*flags,'-I'+str(ROOT),'-I'+str(OUT),str(ROOT/'tests/learning/input_delay.cpp'),'-o',str(exe)])
 p=subprocess.run([str(exe)],text=True,capture_output=True);print(p.stdout,flush=True)
 assert p.returncode==(1 if expect_failure else 0),p.stderr
 if expect_failure:assert 'CHECK' in p.stderr or 'expected0' in p.stderr or 'expected2' in p.stderr
 print('Main policy', 'pre-change failure confirmed' if expect_failure else 'passed',flush=True)
def main():
 OUT.mkdir(parents=True,exist_ok=True)
 prev=CP.parent/'90-input-exchange'
 for p in prev.rglob('*'):
  if p.is_file() and p.relative_to(prev).as_posix() not in {'README.md','DESIGN.md','CMakeLists.txt'}:assert p.read_bytes()==(CP/p.relative_to(prev)).read_bytes(),p
 if '--skip-build' not in sys.argv:
  for backend in ['SCRIPTED','SDL']:
   b=OUT/backend.lower();run(['cmake','-S',str(CP),'-B',str(b),'-DSTUDY_PLATFORM='+backend,'-DCMAKE_BUILD_TYPE=Release','-DCMAKE_CXX_FLAGS=-Wall -Wextra -Wpedantic'])
   p=run(['cmake','--build',str(b),'-j3']);(OUT/f'build-{backend}.log').write_text(p.stdout+p.stderr);assert 'warning:' not in p.stdout+p.stderr,p.stderr
   p=run(['ctest','--test-dir',str(b),'--output-on-failure'],env={**os.environ,'SDL_VIDEODRIVER':'dummy','SDL_AUDIODRIVER':'dummy','DISPLAY':'','WAYLAND_DISPLAY':''});print(backend,p.stdout[-200:],flush=True)
 flags=['c++','-std=c++17','-Wall','-Wextra','-Wpedantic','-fsanitize=address,undefined','-fno-sanitize-recover=all']
 run([*flags,'-I'+str(CP),str(CP/'tests/delay_contract.cpp'),'-o',str(OUT/'delay-sanitized')]);print(run([str(OUT/'delay-sanitized')]).stdout,flush=True)
 timeline=run([str(OUT/'scripted/delay_timeline')]).stdout.splitlines();assert timeline==['pulse remote_max next_D0 next_D2','0 0 1 0','1 1 2 0','2 2 3 1','3 2 3 2','4 2 3 3','5 5 6 4','6 6 7 5','7 7 8 6'];print('\n'.join(timeline),flush=True)
 root_policy();exchanges()
 for b in [ROOT/'out/learning-checkpoints/83-sockets-check/root',ROOT/'out/learning-checkpoints/54-game-adapter-check/root']:
  run(['cmake','--build',str(b),'-j3']);p=run(['ctest','--test-dir',str(b),'--output-on-failure']);print(p.stdout[-250:],flush=True)
 print('Input delay checks complete',flush=True)
if __name__=='__main__':main()
