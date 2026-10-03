"""Lesson 73 settings integration; manuscript review is separate."""
from pathlib import Path
import os, subprocess
from check_learning_text_layout import run
from check_learning_utf8 import cut
ROOT=Path(__file__).resolve().parents[1]
SOURCE=ROOT/'docs/learn/checkpoints/73-settings'
OUT=ROOT/'out/learning-checkpoints/73-settings-check'
def main():
 OUT.mkdir(parents=True,exist_ok=True)
 previous=SOURCE.parent/'72-idle-animation'
 allowed={'CMakeLists.txt','README.md','DESIGN.md','src/main.cpp','client/menu_model.h'}
 for p in previous.rglob('*'):
  if p.is_file() and p.relative_to(previous).as_posix() not in allowed:
   assert p.read_bytes()==(SOURCE/p.relative_to(previous)).read_bytes(),p
 env={**os.environ,'SDL_VIDEODRIVER':'dummy','SDL_AUDIODRIVER':'dummy','DISPLAY':'','WAYLAND_DISPLAY':''}
 for backend in ['SCRIPTED','SDL']:
  build=OUT/backend.lower()
  run(['cmake','-S',str(SOURCE),'-B',str(build),'-DSTUDY_PLATFORM='+backend,'-DCMAKE_BUILD_TYPE=Release','-DCMAKE_CXX_FLAGS=-Wall -Wextra -Wpedantic'])
  result=run(['cmake','--build',str(build),'-j3']);(OUT/f'build-{backend}.log').write_text(result.stdout+result.stderr);assert 'warning:' not in result.stdout+result.stderr
  result=run(['ctest','--test-dir',str(build),'--output-on-failure'],env=env);(OUT/f'ctest-{backend}.log').write_text(result.stdout);print(result.stdout[-350:],flush=True)
 result=run([str(OUT/'sdl/pointer_sdl')],env=env);(OUT/'pointer-sdl.log').write_text(result.stdout+result.stderr);print(result.stdout.strip(),flush=True)
 result=run([str(OUT/'sdl/widget_render'),str(SOURCE/'assets/NanumGothic.ttf')],env={**os.environ,'SDL_VIDEODRIVER':'offscreen','LIBGL_ALWAYS_SOFTWARE':'1'});(OUT/'widget-render.log').write_text(result.stdout+result.stderr);print(result.stdout.strip(),flush=True)
 result=run([str(OUT/'sdl/pointer_pixels')],env={**os.environ,'SDL_VIDEODRIVER':'offscreen','LIBGL_ALWAYS_SOFTWARE':'1'});(OUT/'pointer-pixels.log').write_text(result.stdout+result.stderr);print(result.stdout.strip(),flush=True)
 result=run([str(OUT/'sdl/character_art_real'),str(SOURCE)],env={**os.environ,'SDL_VIDEODRIVER':'offscreen','LIBGL_ALWAYS_SOFTWARE':'1'});(OUT/'character-art-real.log').write_text(result.stdout+result.stderr);print(result.stdout.strip(),flush=True)
 result=run([str(OUT/'sdl/idle_animation_real'),str(SOURCE)],env={**os.environ,'SDL_VIDEODRIVER':'offscreen','LIBGL_ALWAYS_SOFTWARE':'1'});(OUT/'idle-animation-real.log').write_text(result.stdout+result.stderr);print(result.stdout.strip(),flush=True)
 binary=OUT/'ui-sanitized';run(['c++','-std=c++17','-O1','-Wall','-Wextra','-Wpedantic','-fsanitize=undefined,float-cast-overflow','-fno-sanitize-recover=all','-I'+str(SOURCE),str(SOURCE/'tests/settings_contract.cpp'),str(SOURCE/'settings/private_file.cpp'),'-o',str(binary)])
 result=run([str(binary),str(OUT/'sanitized-files')]);(OUT/'sanitized.log').write_text(result.stdout);print(result.stdout.strip(),flush=True)
 result=run(['python3',str(ROOT/'scripts/check_learning_settings_root.py')]);(OUT/'root-art.log').write_text(result.stdout);print(result.stdout.strip(),flush=True)
 result=run(['python3',str(ROOT/'scripts/check_learning_settings_faults.py')]);(OUT/'settings-faults.log').write_text(result.stdout);print(result.stdout.strip(),flush=True)
 build=ROOT/'out/learning-checkpoints/54-game-adapter-check/root'
 result=run(['cmake','--build',str(build),'-j3']);(OUT/'root-build.log').write_text(result.stdout+result.stderr)
 result=run(['ctest','--test-dir',str(build),'--output-on-failure'],env=env);(OUT/'root-ctest.log').write_text(result.stdout);print(result.stdout[-300:],flush=True)
 assert run([str(build/'sim_hash_dump')]).stdout==(ROOT/'python/tests/_sim_hash_dump.txt').read_text()
 print('Settings parsing, file preservation, session changes and cumulative regressions passed.',flush=True)
if __name__=='__main__':main()
