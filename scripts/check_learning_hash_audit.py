"""Lesson92: canonical hash snapshots, bounded pending comparisons and actual main fixes."""
from pathlib import Path
import os,sys,socket,subprocess,re
from check_learning_text_layout import run
from check_learning_utf8 import cut
from check_learning_framing import finish,line,frame
from check_learning_seed import readframe,HELLO,cleanup
from check_learning_lockstep import batch
ROOT=Path(__file__).resolve().parents[1];CP=ROOT/'docs/learn/checkpoints/92-hash-audit';OUT=ROOT/'out/learning-checkpoints/92-hash-audit-check'
def launch(seed=77,delay=2):
 p=subprocess.Popen([str(OUT/'scripted/hash_probe'),'listen','0',str(seed),str(delay)],stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True)
 ready=line(p);assert ready.startswith('LISTEN ');return p,int(ready.split()[1])
def exchanges():
 for seed,delay in [(0,2),(1,0),(77,2),(2**64-1,30)]:
  p,port=launch(seed,delay)
  try:
   peer=run([str(OUT/'scripted/hash_probe'),'connect',str(port)],timeout=10);host=finish(p,0)[0]
   a=re.findall(r'CHECK tick=(\d+) host=match peer=match',host);b=re.findall(r'CHECK tick=(\d+) host=match peer=match',peer.stdout)
   assert a==b==[str(n) for n in range(0,33-delay,4)]
   # End-of-run state outputs are a separate diagnostic from sampled wire comparisons.
   pattern=r'ticks=(\d+) left=(\d+) right=(\d+) delay=(\d+) pending=(\d+)'
   a=re.search(pattern,host);b=re.search(pattern,peer.stdout);assert a and b and a.groups()==b.groups()
  finally:cleanup(p)
 # A scripted endpoint echoes the host's computed stamps to test protocol handling.
 # These cases do not prove independent simulations agree; the real peer above does.
 for fault in ['none','host','peer','missing','early_mismatch','wrong_type','bad_size']:
  p,port=launch()
  try:
   with socket.create_connection(('127.0.0.1',port),timeout=5) as c:
    c.settimeout(5);c.sendall(frame(HELLO,10));kind,config=readframe(c);assert kind==11;c.sendall(frame(config,12))
    kind,initial=readframe(c);assert kind==21
    assert readframe(c)[0]==20 and readframe(c)[0]==20
    if fault=='wrong_type':c.sendall(frame(initial,22));c.shutdown(socket.SHUT_WR);assert finish(p,1)[0].startswith('FAILED');continue
    if fault=='bad_size':c.sendall(frame(initial[:-1],21));c.shutdown(socket.SHUT_WR);assert 'FAILED' in finish(p,1)[0];continue
    c.sendall(frame(initial,21))
    if fault=='early_mismatch':c.sendall(frame((4).to_bytes(8,'little')+bytes(16),21))
    c.sendall(batch(16,[0]*16)+batch(0,[0]*16))
    if fault=='early_mismatch':
     out=finish(p,1)[0];assert 'CHECK tick=4' in out and 'mismatch' in out;continue
    for tick in range(4,29,4):
     kind,payload=readframe(c);assert kind==21 and int.from_bytes(payload[:8],'little')==tick
     if fault=='missing' and tick==4:continue
     data=bytearray(payload)
     if tick==4 and fault in ['host','peer']:data[8 if fault=='host' else 16]^=1
     c.sendall(frame(bytes(data),21))
     if tick==4 and fault in ['host','peer']:break
    c.shutdown(socket.SHUT_WR)
    out=finish(p,0 if fault=='none' else 1)[0]
    if fault in ['host','peer']:assert f'{fault}=mismatch' in out and 'tick=4' in out
    elif fault=='missing':assert 'FAILED' in out and 'CHECK tick=4' not in out
  finally:cleanup(p)
 print('HASH wire: four independent peers/sample sequences; seven controlled match/mismatch/early/missing/type/size cases passed',flush=True)
def root_probe(source=None,before=False):
 # Current production comparison moved to a bounded mailbox in lesson97.
 if not before and source is None and 'PollHashComparison' in (ROOT/'src/main.cpp').read_text():
  from check_learning_hash_observation import root_probe as current_root_probe
  current_root_probe()
  return
 s=(source or ROOT/'src/main.cpp').read_text()
 a=s.index('struct HashSnap {');b=s.index('// Section I — 공격',a)
 (OUT/'main_hash_storage.inc').write_text(s[a:b])
 a=s.index('// F.2 — 원격 HASH 수신');s=s[a:]
 (OUT/'main_hash_compare.inc').write_text(cut(s,'if (app == AppMode::Net && gameLocal)'))
 flags=['c++','-std=c++17','-Wall','-Wextra','-Wpedantic','-fsanitize=address,undefined','-fno-sanitize-recover=all']
 exe=OUT/('arrival-before' if before else 'arrival-after')
 run([*flags,*([] if before else ['-DCHECK_RESET']),'-I'+str(OUT),str(ROOT/'tests/learning/hash_arrival.cpp'),'-o',str(exe)])
 p=subprocess.run([str(exe)],text=True,capture_output=True);assert p.returncode==(1 if before else 0),p.stderr
 print('Actual main', 'before rejected by early-arrival assertion' if before else p.stdout.strip(),flush=True)
 if before:assert 'lastRemoteHashSeenTick==0' in p.stderr
 else:assert '[DESYNC-DIAGNOSTIC] captured_tick=601 compared_tick=600' in p.stderr
 # Both game creation paths explicitly reset comparison state.
 if not before:
  source=(ROOT/'src/main.cpp').read_text();assert source.count('resetHashComparison();')==2
  assert 'hash_player_pair(hL, hR)' in source and 'hash_player_pair(hR, hL)' in source
  assert 'uint64_t h  = hL ^ hR;' not in source
 def pair_test():
  run([*flags,'-I'+str(ROOT),str(ROOT/'tests/learning/hash_pair.cpp'),'-o',str(OUT/'pair')]);print(run([str(OUT/'pair')]).stdout,flush=True)
 if not before:pair_test()
def main():
 OUT.mkdir(parents=True,exist_ok=True);prev=CP.parent/'91-input-delay'
 for p in prev.rglob('*'):
  if p.is_file() and p.relative_to(prev).as_posix() not in {'README.md','DESIGN.md','CMakeLists.txt'}:assert p.read_bytes()==(CP/p.relative_to(prev)).read_bytes(),p
 if '--skip-build' not in sys.argv:
  for backend in ['SCRIPTED','SDL']:
   b=OUT/backend.lower();run(['cmake','-S',str(CP),'-B',str(b),'-DSTUDY_PLATFORM='+backend,'-DCMAKE_BUILD_TYPE=Release','-DCMAKE_CXX_FLAGS=-Wall -Wextra -Wpedantic'])
   p=run(['cmake','--build',str(b),'-j3']);(OUT/f'build-{backend}.log').write_text(p.stdout+p.stderr);assert 'warning:' not in p.stdout+p.stderr,p.stderr
   p=run(['ctest','--test-dir',str(b),'--output-on-failure'],env={**os.environ,'SDL_VIDEODRIVER':'dummy','SDL_AUDIODRIVER':'dummy','DISPLAY':'','WAYLAND_DISPLAY':''});print(backend,p.stdout[-200:],flush=True)
 flags=['c++','-std=c++17','-Wall','-Wextra','-Wpedantic','-fsanitize=address,undefined','-fno-sanitize-recover=all']
 run([*flags,'-I'+str(CP),str(CP/'tests/hash_audit_contract.cpp'),'-o',str(OUT/'audit-sanitized')]);print(run([str(OUT/'audit-sanitized')]).stdout,flush=True)
 root_probe();exchanges()
 for b in [ROOT/'out/learning-checkpoints/83-sockets-check/root',ROOT/'out/learning-checkpoints/54-game-adapter-check/root']:
  run(['cmake','--build',str(b),'-j3']);p=run(['ctest','--test-dir',str(b),'--output-on-failure']);print(p.stdout[-250:],flush=True)
 print('Hash audit checks complete',flush=True)
if __name__=='__main__':main()
