"""Lesson 81: full XAudio implementation with an API double, not native Windows."""
from pathlib import Path
import os,subprocess
from check_learning_text_layout import run
ROOT=Path(__file__).resolve().parents[1]
SOURCE=ROOT/'docs/learn/checkpoints/81-xaudio2'
OUT=ROOT/'out/learning-checkpoints/81-xaudio2-check'
def main():
 OUT.mkdir(parents=True,exist_ok=True)
 prev=SOURCE.parent/'80-voice-pool'
 changed={'CMakeLists.txt','README.md','DESIGN.md','presentation/sound_session.h','platform/sdl.cpp','audio/player.cpp'}
 for p in prev.rglob('*'):
  if p.is_file() and p.relative_to(prev).as_posix() not in changed:
   if p.suffix not in {'.cpp','.h','.md','.txt'}:
    assert p.read_bytes()==(SOURCE/p.relative_to(prev)).read_bytes(),p
    continue
   current=(SOURCE/p.relative_to(prev)).read_text()
   if p.suffix=='.cpp' and p.parent.name=='tests':
    current=current.replace('\n    SDL_SetMainReady(); // Explicit console entry (SDL_MAIN_HANDLED).','')
   if p.suffix in {'.cpp','.h','.md','.txt'}: assert p.read_text()==current,p
   else: assert p.read_bytes()==(SOURCE/p.relative_to(prev)).read_bytes(),p
 env={**os.environ,'SDL_VIDEODRIVER':'dummy','SDL_AUDIODRIVER':'dummy','DISPLAY':'','WAYLAND_DISPLAY':''}
 for backend in ['SCRIPTED','SDL']:
  build=OUT/backend.lower();run(['cmake','-S',str(SOURCE),'-B',str(build),'-DSTUDY_PLATFORM='+backend,'-DCMAKE_BUILD_TYPE=Release','-DCMAKE_CXX_FLAGS=-Wall -Wextra -Wpedantic'])
  r=run(['cmake','--build',str(build),'-j3']);(OUT/f'build-{backend}.log').write_text(r.stdout+r.stderr);assert 'warning:' not in r.stdout+r.stderr
  r=run(['ctest','--test-dir',str(build),'--output-on-failure'],env=env);(OUT/f'ctest-{backend}.log').write_text(r.stdout);print(r.stdout[-350:],flush=True)
 for platform,audio in [('SDL','NONE'),('SCRIPTED','SDL')]:
  build=OUT/(platform.lower()+'-'+audio.lower());run(['cmake','-S',str(SOURCE),'-B',str(build),'-DSTUDY_PLATFORM='+platform,'-DSTUDY_AUDIO='+audio,'-DCMAKE_BUILD_TYPE=Release'])
  run(['cmake','--build',str(build),'--target','sound_session_contract','-j3'])
  r=run(['ctest','--test-dir',str(build),'-R','^sound_session_','--output-on-failure'],env=env);print(r.stdout[-220:],flush=True)
 for audio in ['invalid','XAUDIO2']:
  r=subprocess.run(['cmake','-S',str(SOURCE),'-B',str(OUT/('reject-'+audio)),'-DSTUDY_PLATFORM=SCRIPTED','-DSTUDY_AUDIO='+audio],text=True,capture_output=True)
  assert r.returncode!=0 and ('Unknown STUDY_AUDIO' if audio=='invalid' else 'requires a Windows target') in r.stderr
 for name in ['xaudio_contract','xaudio_session_contract']:
  binary=OUT/(name+'-sanitized');run(['c++','-std=c++17','-Wall','-Wextra','-Wpedantic','-O1','-fsanitize=address,undefined','-fno-sanitize-recover=all','-DSTUDY_AUDIO_XAUDIO2','-I'+str(SOURCE/'tests/xaudio_fake'),'-I'+str(SOURCE),str(SOURCE/('tests/'+name+'.cpp')),str(SOURCE/'audio/xaudio_player.cpp'),'-o',str(binary)])
  r=run([str(binary)],env=env);(OUT/(name+'-sanitized.log')).write_text(r.stdout+r.stderr);print(r.stdout.strip(),flush=True)
 # Compile the actual game caller with the Windows-selected Session against the double.
 run(['c++','-std=c++17','-Wall','-Wextra','-Wpedantic','-DSTUDY_AUDIO_XAUDIO2','-I'+str(SOURCE/'tests/xaudio_fake'),'-I'+str(SOURCE),'-c',str(SOURCE/'src/main.cpp'),'-o',str(OUT/'main-xaudio-double.o')])
 build=ROOT/'out/learning-checkpoints/54-game-adapter-check/root'
 run(['cmake','--build',str(build),'-j3'])
 r=run(['ctest','--test-dir',str(build),'--output-on-failure'],env=env);(OUT/'root-ctest.log').write_text(r.stdout);print(r.stdout[-300:],flush=True)
 assert run([str(build/'sim_hash_dump')]).stdout==(ROOT/'python/tests/_sim_hash_dump.txt').read_text()
 print('Cumulative XAudio/API-double and audio build-selection checks passed.',flush=True)
if __name__=='__main__':main()
