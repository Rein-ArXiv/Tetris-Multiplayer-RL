"""Lesson96: lifecycle gates and round-tagged input reuse without stale insertion."""
from pathlib import Path
import sys,os,subprocess,json
from check_learning_text_layout import run
from check_learning_utf8 import cut
from check_part_docs import normalized
ROOT=Path(__file__).resolve().parents[1];CP=ROOT/'docs/learn/checkpoints/96-round-inputs';OUT=ROOT/'out/learning-checkpoints/96-round-inputs-check'
def main():
 OUT.mkdir(parents=True,exist_ok=True)
 for p in (CP.parent/'95-backpressure').rglob('*'):
  if p.is_file() and p.relative_to(CP.parent/'95-backpressure').as_posix() not in {'CMakeLists.txt','README.md','DESIGN.md'}:
   assert p.read_bytes()==(CP/p.relative_to(CP.parent/'95-backpressure')).read_bytes(),p
 if '--skip-build' not in sys.argv:
  for backend in ['SCRIPTED','SDL']:
   b=OUT/backend.lower();run(['cmake','-S',str(CP),'-B',str(b),'-DSTUDY_PLATFORM='+backend,'-DCMAKE_BUILD_TYPE=Release','-DCMAKE_CXX_FLAGS=-Wall -Wextra -Wpedantic'])
   p=run(['cmake','--build',str(b),'-j3']);(OUT/f'build-{backend}.log').write_text(p.stdout+p.stderr);assert 'warning:' not in p.stdout+p.stderr
   p=run(['ctest','--test-dir',str(b),'--output-on-failure'],env={**os.environ,'SDL_VIDEODRIVER':'dummy','SDL_AUDIODRIVER':'dummy','DISPLAY':'','WAYLAND_DISPLAY':''});print(backend,p.stdout[-225:],flush=True)
 # A bad implementation that admits old-round frames must fail the fresh-round history check.
 mutant=OUT/'mutant/net';mutant.mkdir(parents=True,exist_ok=True)
 source=(CP/'net/round_play.h').read_text()
 old='if (scope != RoundScope::current) return {scope, Put::invalid};'
 assert old in source
 (mutant/'round_play.h').write_text(source.replace(old,'if (scope != RoundScope::current && scope != RoundScope::old_round) return {scope, Put::invalid};'))
 run(['c++','-std=c++17','-I'+str(mutant.parent),'-I'+str(CP),str(CP/'tests/round_contract.cpp'),'-o',str(OUT/'round-mutant')])
 p=subprocess.run([str(OUT/'round-mutant')],text=True,capture_output=True,timeout=15)
 assert p.returncode!=0 and 'peer.receive(record(2,0,2)).input==Put::stored' in p.stderr,p.stderr
 print('Old-round admission mutation rejected before fresh-round simulation',flush=True)
 flags=['c++','-std=c++17','-Wall','-Wextra','-Wpedantic','-fsanitize=address,undefined','-fno-sanitize-recover=all']
 run([*flags,'-I'+str(CP),str(CP/'tests/round_contract.cpp'),'-o',str(OUT/'round-sanitized')]);print(run([str(OUT/'round-sanitized')]).stdout,flush=True)
 for before in [True,False]:
  source=ROOT/('out/learning-jobs/096-before-session.cpp' if before else 'net/session.cpp')
  (OUT/'input_ready.inc').write_text(cut(source.read_text(),'void Session::SendInput('))
  exe=OUT/('ready-before' if before else 'ready-after')
  run([*flags,'-I'+str(ROOT),'-I'+str(OUT),str(ROOT/'tests/learning/input_ready.cpp'),str(ROOT/'net/framing.cpp'),'-o',str(exe)])
  p=subprocess.run([str(exe)],text=True,capture_output=True,timeout=10);assert p.returncode==(1 if before else 0),p.stderr
  print('readiness before rejected' if before else p.stdout.strip(),flush=True)
 assert 'if (session.isReady() && gameLocal && gameRemote && startDelay == 0 &&' in (ROOT/'src/main.cpp').read_text()
 lesson=ROOT/'docs/learn/lessons/096.json'
 if lesson.exists():
  corpora={lang:[normalized(p.read_text(),lang) for p in CP.rglob('*') if p.is_file() and (p.suffix in ['.cpp','.h'] if lang=='cpp' else p.name=='CMakeLists.txt')] for lang in ['cpp','cmake']};count=0
  for sec in json.loads(lesson.read_text())['sections']:
   for code in sec.get('codes',[]):
    if 'text' in code and code['language'] in corpora:
     assert any(normalized(code['text'],code['language']) in s for s in corpora[code['language']]),code['label'];count+=1
  print('Inline implementations',count,flush=True)
 for b in [ROOT/'out/learning-checkpoints/83-sockets-check/root',ROOT/'out/learning-checkpoints/54-game-adapter-check/root']:
  run(['cmake','--build',str(b),'-j3']);p=run(['ctest','--test-dir',str(b),'--output-on-failure']);print(p.stdout[-210:],flush=True)
 print('Round input checks complete',flush=True)
if __name__=='__main__':main()
