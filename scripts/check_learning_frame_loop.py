"""Check the frame/simulation boundary, snapshots and real GPU results."""
from pathlib import Path
import os,subprocess
ROOT=Path(__file__).resolve().parents[1]
SOURCE=ROOT/'docs/learn/checkpoints/44-frame-loop'
OUT=ROOT/'out/learning-checkpoints/44-frame-loop-check'
def run(args,**kw):
 r=subprocess.run(args,cwd=ROOT,text=True,capture_output=True,timeout=kw.pop('timeout',60),**kw)
 if r.returncode:raise RuntimeError(f'{args}\n{r.stdout}\n{r.stderr}')
 return r

def main():
 OUT.mkdir(parents=True,exist_ok=True)
 env={k:v for k,v in os.environ.items() if k not in ['LD_PRELOAD','LEARN_GL','LEARN_SMOKE']}
 expected='frame=1 ticks=0 locks=0 boardChanged=0 score=0 lastSpin=-1\nframe=2 ticks=1 locks=1 boardChanged=1 score=800 lastSpin=1\nframe=3 ticks=3 locks=0 boardChanged=0 score=800 lastSpin=-1'
 for backend in ['SCRIPTED','SDL']:
  build=OUT/backend.lower()
  run(['cmake','-S',str(SOURCE),'-B',str(build),'-DSTUDY_PLATFORM='+backend,'-DCMAKE_BUILD_TYPE=Release','-DCMAKE_CXX_FLAGS=-Wall -Wextra -Wpedantic'])
  r=run(['cmake','--build',str(build),'-j3'],timeout=240);(OUT/f'build-{backend}.log').write_text(r.stdout+r.stderr);assert 'warning:' not in r.stdout+r.stderr
  r=run(['ctest','--test-dir',str(build),'--output-on-failure']);(OUT/f'ctest-{backend}.log').write_text(r.stdout);print(r.stdout.strip(),flush=True)
  for name in ['frame_demo','frame_contract']:
   link=(build/'CMakeFiles'/f'{name}.dir/link.txt').read_text();assert not any(s in link for s in ['SDL','study_gl','study_platform'])
   r=run([str(build/name)],env={**env,'DISPLAY':'','WAYLAND_DISPLAY':'','SDL_VIDEODRIVER':'unavailable'});(OUT/f'{name}-{backend}.log').write_text(r.stdout)
   if name=='frame_demo':assert r.stdout.strip()==expected,r.stdout
   print(r.stdout.strip(),flush=True)
  if backend=='SDL':
   r=run([str(build/'frame_probe'),str(OUT/'frame')],env={**env,'SDL_VIDEODRIVER':'offscreen'},timeout=90);(OUT/'gl.log').write_text(r.stdout);assert r.stdout.count('all framebuffer RGB matched')==18
   print('18 GL frames through FrameRunner: 0-tick capture then 1-tick update, all board/active/ghost VBO and RGB matched',flush=True)
 flags=['-std=c++17','-O1','-DNDEBUG','-fsanitize=undefined','-fno-sanitize-recover=all']
 run(['c++',*flags,'-I'+str(SOURCE),str(SOURCE/'tests/frame_contract.cpp'),'-o',str(OUT/'sanitized')]);print(run([str(OUT/'sanitized')]).stdout,flush=True)
 for name,old,new in [('drop_zero_tick','candidate.pending_.capture(', 'if(batch->ticks) candidate.pending_.capture('),('overwrite_events','report.observations[i] = observation;','report.observations[0] = observation;'),('lose_board_flag','report.board_changed = true;','report.board_changed = false;')]:
  mutation=OUT/name;(mutation/'loop').mkdir(parents=True,exist_ok=True)
  source=(SOURCE/'loop/frame_runner.h').read_text();assert old in source
  (mutation/'loop/frame_runner.h').write_text(source.replace(old,new))
  run(['c++','-std=c++17','-O1','-DNDEBUG','-I'+str(mutation),'-I'+str(SOURCE),str(SOURCE/'tests/frame_contract.cpp'),'-o',str(mutation/'check')])
  r=subprocess.run([str(mutation/'check')],capture_output=True,text=True,timeout=10);assert r.returncode==1 and 'frame line' in r.stderr
 print('Zero-tick loss, overwritten reports and missing board upload mutations rejected in Release',flush=True)
if __name__=='__main__':main()
