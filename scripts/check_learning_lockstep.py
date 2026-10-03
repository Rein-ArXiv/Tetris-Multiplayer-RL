"""Lesson90: exact tick pairs, atomic input windows and two-process duel replay."""
from pathlib import Path
import os,sys,socket,subprocess,re
from check_learning_text_layout import run
from check_learning_framing import finish,line,frame,exact
from check_learning_seed import readframe,HELLO,cleanup
ROOT=Path(__file__).resolve().parents[1]
CP=ROOT/'docs/learn/checkpoints/90-input-exchange'
OUT=ROOT/'out/learning-checkpoints/90-input-exchange-check'
def launch(seed=77):
 p=subprocess.Popen([str(OUT/'scripted/input_probe'),'listen','0',str(seed)],stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True)
 text=line(p);assert text.startswith('LISTEN ');return p,int(text.split()[1])
def batch(first,values,kind=20):
 return frame(first.to_bytes(4,'little')+len(values).to_bytes(2,'little')+bytes(values),kind)
def exchanges():
 for seed in [0,1,77,2**64-1]:
  p,port=launch(seed)
  try:
   peer=run([str(OUT/'scripted/input_probe'),'connect',str(port)],timeout=10)
   host=finish(p,0)[0]
   pattern=r'ticks=(\d+) left=(\d+) right=(\d+)'
   a=re.search(pattern,host);b=re.search(pattern,peer.stdout)
   assert a and b and a.groups()==b.groups() and int(a[1])==12
   assert 'WAIT next=0' in host and 'WAIT next=0' in peer.stdout
  finally:cleanup(p)
 # Custom peer keeps tick0 missing until the final frame. Duplicates are
 # accepted only while a tick is pending; consumed ticks are deliberately stale.
 future=batch(6,[0]*6);early=batch(0,[0]*6)
 cases=[('hole',future,1,0),('duplicate',future+future+early,0,12),
        ('conflict',future+batch(6,[1]*6),1,0),('wrong_type',batch(0,[0],1),1,0),
        ('invalid_mask',batch(0,[32]),1,0),('range',batch(12,[0]),1,0),
        ('no_input',b'',1,0),('stale',future+early+early,1,12),
        ('truncated',future+b'\x05',1,0)]
 for name,data,code,tick in cases:
  p,port=launch()
  try:
   with socket.create_connection(('127.0.0.1',port),timeout=5) as c:
    c.settimeout(5);c.sendall(frame(HELLO,10));kind,config=readframe(c)
    assert kind==11 and config[-6:-2]==bytes(4) and config[-2]==0
    c.sendall(frame(config,12));assert readframe(c)[0]==20;assert readframe(c)[0]==20
    c.sendall(data);c.shutdown(socket.SHUT_WR)
    output=finish(p,code)[0]
    assert (f'FAILED next={tick}' if code else 'ticks=12') in output,(name,output)
  finally:cleanup(p)
 print('Two-process four-seed ordered board hashes; future hole waiting; nine peer duplicate/conflict/missing/type/range/truncation cases passed',flush=True)
def main():
 OUT.mkdir(parents=True,exist_ok=True)
 prev=CP.parent/'89-seed-handshake'
 for p in prev.rglob('*'):
  if p.is_file() and p.relative_to(prev).as_posix() not in {'README.md','DESIGN.md','CMakeLists.txt'}:assert p.read_bytes()==(CP/p.relative_to(prev)).read_bytes(),p
 if '--skip-build' not in sys.argv:
  for backend in ['SCRIPTED','SDL']:
   b=OUT/backend.lower();run(['cmake','-S',str(CP),'-B',str(b),'-DSTUDY_PLATFORM='+backend,'-DCMAKE_BUILD_TYPE=Release','-DCMAKE_CXX_FLAGS=-Wall -Wextra -Wpedantic'])
   p=run(['cmake','--build',str(b),'-j3']);(OUT/f'build-{backend}.log').write_text(p.stdout+p.stderr);assert 'warning:' not in p.stdout+p.stderr,p.stderr
   p=run(['ctest','--test-dir',str(b),'--output-on-failure'],env={**os.environ,'SDL_VIDEODRIVER':'dummy','SDL_AUDIODRIVER':'dummy','DISPLAY':'','WAYLAND_DISPLAY':''});print(backend,p.stdout[-200:],flush=True)
 flags=['c++','-std=c++17','-Wall','-Wextra','-Wpedantic','-fsanitize=address,undefined','-fno-sanitize-recover=all']
 for source,include,name in [(CP/'tests/lockstep_contract.cpp',CP,'lockstep'),(ROOT/'tests/learning/input_pair.cpp',ROOT,'input-pair')]:
  run([*flags,'-I'+str(include),str(source),'-o',str(OUT/name)]);print(run([str(OUT/name)]).stdout,flush=True)
 source=(ROOT/'src/main.cpp').read_text()
 start=source.index('while ((int64_t)simTick <= safeTick &&')
 end=source.index('gameRemote->Tick();',start)+len('gameRemote->Tick();')
 (OUT/'main_input_loop.inc').write_text(source[start:end])
 run([*flags,'-I'+str(ROOT),'-I'+str(OUT),str(ROOT/'tests/learning/lockstep_loop.cpp'),'-o',str(OUT/'main-loop')])
 print(run([str(OUT/'main-loop')]).stdout,flush=True)
 exchanges()
 for b in [ROOT/'out/learning-checkpoints/83-sockets-check/root',ROOT/'out/learning-checkpoints/54-game-adapter-check/root']:
  run(['cmake','--build',str(b),'-j3']);p=run(['ctest','--test-dir',str(b),'--output-on-failure']);print(p.stdout[-250:],flush=True)
 print('Lockstep checks complete',flush=True)
if __name__=='__main__':main()
