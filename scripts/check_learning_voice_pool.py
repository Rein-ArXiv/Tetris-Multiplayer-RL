"""Lesson 80 voice pool checks; speaker output and native Windows are separate."""
from pathlib import Path
import os,shlex
from check_learning_text_layout import run
ROOT=Path(__file__).resolve().parents[1]
SOURCE=ROOT/'docs/learn/checkpoints/80-voice-pool'
OUT=ROOT/'out/learning-checkpoints/80-voice-pool-check'
def main():
 OUT.mkdir(parents=True,exist_ok=True)
 prev=SOURCE.parent/'79-sound-events'
 for p in prev.rglob('*'):
  if p.is_file() and p.relative_to(prev).as_posix() not in {'CMakeLists.txt','README.md','DESIGN.md','audio/player.cpp','audio/player.h','src/main.cpp','tests/callback_contract.cpp','presentation/sound_session.h'}:
   assert p.read_bytes()==(SOURCE/p.relative_to(prev)).read_bytes(),p
 env={**os.environ,'SDL_VIDEODRIVER':'dummy','SDL_AUDIODRIVER':'dummy','DISPLAY':'','WAYLAND_DISPLAY':''}
 for backend in ['SCRIPTED','SDL']:
  build=OUT/backend.lower();run(['cmake','-S',str(SOURCE),'-B',str(build),'-DSTUDY_PLATFORM='+backend,'-DCMAKE_BUILD_TYPE=Release','-DCMAKE_CXX_FLAGS=-Wall -Wextra -Wpedantic'])
  r=run(['cmake','--build',str(build),'-j3']);(OUT/f'build-{backend}.log').write_text(r.stdout+r.stderr);assert 'warning:' not in r.stdout+r.stderr
  r=run(['ctest','--test-dir',str(build),'--output-on-failure'],env=env);(OUT/f'ctest-{backend}.log').write_text(r.stdout);print(r.stdout[-350:],flush=True)
 flags=shlex.split(run(['pkg-config','--cflags','--libs','sdl2']).stdout)
 for name,extra,args in [('voice_contract',[],[]),('playback_contract',['audio/player.cpp','audio/mp3_decode.cpp'],[str(SOURCE/'assets/rotate.mp3')]),('callback_contract',[],[]),('mixing_contract',[],[]),('voice_pool_contract',[],[]),('sound_events_contract',[],[]),('cue_contract',[],[]),('sound_session_contract',['audio/player.cpp'],[])]:
  binary=OUT/(name+'-sanitized');run(['c++','-std=c++17','-O1','-fsanitize=address,undefined,float-cast-overflow','-fno-sanitize-recover=all','-DSTUDY_AUDIO_SDL','-I'+str(SOURCE),str(SOURCE/('tests/'+name+'.cpp')),*[str(SOURCE/f) for f in extra],*flags,'-o',str(binary)])
  r=run([str(binary),*args],env=env);(OUT/(name+'-sanitized.log')).write_text(r.stdout+r.stderr);print((r.stdout+r.stderr).strip(),flush=True)
 build=ROOT/'out/learning-checkpoints/54-game-adapter-check/root'
 r=run(['cmake','--build',str(build),'-j3']);(OUT/'root-build.log').write_text(r.stdout+r.stderr)
 r=run(['ctest','--test-dir',str(build),'--output-on-failure'],env=env);(OUT/'root-ctest.log').write_text(r.stdout);print(r.stdout[-300:],flush=True)
 assert run([str(build/'sim_hash_dump')]).stdout==(ROOT/'python/tests/_sim_hash_dump.txt').read_text()
 print('Voice pool and cumulative regressions passed.',flush=True)
if __name__=='__main__':main()
