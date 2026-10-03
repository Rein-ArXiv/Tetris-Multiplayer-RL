"""Lesson 67 integration checks; manuscript review remains a separate step."""
from pathlib import Path
import os
from check_learning_text_layout import run,root_probe
ROOT=Path(__file__).resolve().parents[1]
SOURCE=ROOT/'docs/learn/checkpoints/67-font-cache'
OUT=ROOT/'out/learning-checkpoints/67-font-cache-check'
def main():
 OUT.mkdir(parents=True,exist_ok=True)
 previous=SOURCE.parent/'66-text-layout';allowed={'CMakeLists.txt','README.md','src/main.cpp','text/font.h','text/font.cpp'}
 for p in previous.rglob('*'):
  if p.is_file() and p.relative_to(previous).as_posix() not in allowed:
   assert p.read_bytes()==(SOURCE/p.relative_to(previous)).read_bytes(),p
 env={**os.environ,'SDL_VIDEODRIVER':'dummy','SDL_AUDIODRIVER':'dummy','DISPLAY':'','WAYLAND_DISPLAY':''}
 for backend in ['SCRIPTED','SDL']:
  build=OUT/backend.lower();run(['cmake','-S',str(SOURCE),'-B',str(build),'-DSTUDY_PLATFORM='+backend,'-DCMAKE_BUILD_TYPE=Release','-DCMAKE_CXX_FLAGS=-Wall -Wextra -Wpedantic'])
  r=run(['cmake','--build',str(build),'-j3']);(OUT/f'build-{backend}.log').write_text(r.stdout+r.stderr);assert 'warning:' not in r.stdout+r.stderr
  r=run(['ctest','--test-dir',str(build),'--output-on-failure'],env=env);(OUT/f'ctest-{backend}.log').write_text(r.stdout);print(r.stdout[-350:],flush=True)
 r=run([str(OUT/'sdl/cache_real'),str(SOURCE/'assets/NanumGothic.ttf')],env={**env,'SDL_VIDEODRIVER':'offscreen','LIBGL_ALWAYS_SOFTWARE':'1'});(OUT/'pixels.log').write_text(r.stdout+r.stderr);print(r.stdout.strip(),flush=True)
 r=run([str(OUT/'sdl/text_layout_demo'),str(SOURCE/'assets/NanumGothic.ttf')]);(OUT/'demo.log').write_text(r.stdout);print(r.stdout.strip(),flush=True)
 for target in ['cache_contract','gpu_cache_contract','glyph_contract']:
  binary=OUT/(target+'-sanitized')
  run(['c++','-std=c++17','-O1','-Wall','-Wextra','-Wpedantic','-fsanitize=undefined,float-cast-overflow','-fno-sanitize-recover=all','-I'+str(SOURCE),'-isystem',str(SOURCE/'third_party'),str(SOURCE/'text/font.cpp'),str(SOURCE/('tests/'+target+'.cpp')),'-o',str(binary)])
  r=run([str(binary),str(SOURCE/'assets/NanumGothic.ttf')]);(OUT/(target+'-sanitized.log')).write_text(r.stdout);print(r.stdout.strip(),flush=True)
 root_probe()
 build=ROOT/'out/learning-checkpoints/54-game-adapter-check/root';r=run(['cmake','--build',str(build),'-j3']);(OUT/'root-build.log').write_text(r.stdout+r.stderr)
 r=run(['ctest','--test-dir',str(build),'--output-on-failure'],env=env);(OUT/'root-ctest.log').write_text(r.stdout);print(r.stdout[-300:],flush=True)
 assert run([str(build/'sim_hash_dump')]).stdout==(ROOT/'python/tests/_sim_hash_dump.txt').read_text()
 print('Font CPU/GPU caches, DPI pixels, lifetime, retry and inherited regressions passed.',flush=True)
if __name__=='__main__':main()
