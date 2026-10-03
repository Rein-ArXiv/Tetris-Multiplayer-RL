"""Real Game wrapper event consumption: old delayed flags vs immediate drain."""
from pathlib import Path
from check_learning_text_layout import run
import subprocess
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'out/learning-checkpoints/79-sound-events-check/root-events'
def main():
 OUT.mkdir(parents=True,exist_ok=True)
 source=(ROOT/'src/game.cpp').read_text()
 a=source.index('void Game::SubmitInput');b=source.index('unsigned long long Game::ComputeStateHash',a)
 before=OUT/'before.cpp';before.write_text(source[:a]+(ROOT/'tests/learning/legacy_game_sound_dispatch.inc').read_text()+source[b:])
 common=[str(ROOT/p) for p in ['tests/game_wrapper_test.cpp','src/colors.cpp','src/sim_game.cpp','src/position.cpp']]
 for name,cpp in [('before',before),('after',ROOT/'src/game.cpp')]:
  binary=OUT/name
  run(['c++','-std=c++17','-O1','-g','-fsanitize=address,undefined','-fno-sanitize-recover=all','-I'+str(ROOT),'-I'+str(ROOT/'src'),str(cpp),*common,'-o',str(binary)])
  r=subprocess.run([str(binary)],capture_output=True,text=True)
  (OUT/(name+'.log')).write_text(r.stdout+r.stderr)
  if name=='before':
   assert r.returncode!=0 and 'plays.size()==before+2' in r.stderr,(r.returncode,r.stderr)
   print('before: actual hard drop left garbage request until a later Tick',flush=True)
  else:
   assert r.returncode==0,(r.returncode,r.stderr)
   print(r.stdout.strip(),flush=True)
if __name__=='__main__':main()
