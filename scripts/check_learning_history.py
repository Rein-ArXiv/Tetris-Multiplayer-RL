"""Check rotation eligibility, clear streaks and actual rendered lock results."""
from pathlib import Path
import os,subprocess
ROOT=Path(__file__).resolve().parents[1]
SOURCE=ROOT/'docs/learn/checkpoints/43-history'
OUT=ROOT/'out/learning-checkpoints/43-history-check'
def run(args,**kw):
 r=subprocess.run(args,cwd=ROOT,text=True,capture_output=True,timeout=kw.pop('timeout',60),**kw)
 if r.returncode:raise RuntimeError(f'{args}\n{r.stdout}\n{r.stderr}')
 return r

def main():
 OUT.mkdir(parents=True,exist_ok=True)
 env={k:v for k,v in os.environ.items() if k not in ['LD_PRELOAD','LEARN_GL','LEARN_SMOKE']}
 for backend in ['SCRIPTED','SDL']:
  build=OUT/backend.lower()
  run(['cmake','-S',str(SOURCE),'-B',str(build),'-DSTUDY_PLATFORM='+backend,'-DCMAKE_BUILD_TYPE=Release','-DCMAKE_CXX_FLAGS=-Wall -Wextra -Wpedantic'])
  r=run(['cmake','--build',str(build),'-j3'],timeout=240);(OUT/f'build-{backend}.log').write_text(r.stdout+r.stderr);assert 'warning:' not in r.stderr
  r=run(['ctest','--test-dir',str(build),'--output-on-failure']);(OUT/f'ctest-{backend}.log').write_text(r.stdout);print(r.stdout.strip(),flush=True)
  for name in ['history_demo','history_contract']:
   link=(build/'CMakeFiles'/f'{name}.dir/link.txt').read_text();assert not any(s in link for s in ['SDL','study_gl','study_platform'])
   r=run([str(build/name)],env={**env,'DISPLAY':'','WAYLAND_DISPLAY':'','SDL_VIDEODRIVER':'unavailable'});(OUT/f'{name}-{backend}.log').write_text(r.stdout);print(r.stdout.strip(),flush=True)
  if backend=='SDL':
   r=run([str(build/'history_probe'),str(OUT/'frame')],env={**env,'SDL_VIDEODRIVER':'offscreen'},timeout=90);(OUT/'gl.log').write_text(r.stdout);assert r.stdout.count('all framebuffer RGB matched')==18
   print('18 GL frames: T-spin 0/1/2 before/after x three sizes, all RGB and VBO matched',flush=True)
 flags=['-std=c++17','-O1','-DNDEBUG','-fsanitize=undefined','-fno-sanitize-recover=all']
 run(['c++',*flags,'-I'+str(SOURCE),str(SOURCE/'tests/history_contract.cpp'),'-o',str(OUT/'sanitized')]);print(run([str(OUT/'sanitized')]).stdout,flush=True)
 run(['c++',*flags,'-I'+str(ROOT),str(ROOT/'tests/sim_t_spin_test.cpp'),str(ROOT/'src/sim_game.cpp'),str(ROOT/'src/position.cpp'),'-o',str(OUT/'current_sanitized')]);print(run([str(OUT/'current_sanitized')]).stdout,flush=True)
 for name,file,old,new in [('forget_idle','round.h','Round candidate = *this;','Round candidate = *this; candidate.rotation_ready_ = false;'),('two_corners','t_spin.h','return blocked >= 3;','return blocked >= 2;'),('no_reset','history.h','after.clears = 0;','after.clears = before.clears;')]:
  mutation=OUT/name;(mutation/'simulation').mkdir(parents=True,exist_ok=True)
  source=(SOURCE/'simulation'/file).read_text();assert old in source
  (mutation/'simulation'/file).write_text(source.replace(old,new))
  run(['c++','-std=c++17','-O1','-DNDEBUG','-I'+str(mutation),'-I'+str(SOURCE),str(SOURCE/'tests/history_contract.cpp'),'-o',str(mutation/'check')])
  r=subprocess.run([str(mutation/'check')],capture_output=True,text=True,timeout=10);assert r.returncode==1 and 'history line' in r.stderr
 print('Three history/corner/streak mutations rejected in Release',flush=True)
if __name__=='__main__':main()
