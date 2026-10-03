"""Lesson 75: real MP3 decoding, bounded output and cumulative builds."""
from pathlib import Path
import os
from check_learning_text_layout import run
ROOT=Path(__file__).resolve().parents[1]
SOURCE=ROOT/'docs/learn/checkpoints/75-mp3'
OUT=ROOT/'out/learning-checkpoints/75-mp3-check'
def main():
 OUT.mkdir(parents=True,exist_ok=True)
 previous=SOURCE.parent/'74-pcm'
 for p in previous.rglob('*'):
  if p.is_file() and p.relative_to(previous).as_posix() not in {'CMakeLists.txt','README.md','DESIGN.md'}:
   assert p.read_bytes()==(SOURCE/p.relative_to(previous)).read_bytes(),p
 for f in ['mp3_decode.cpp','mp3_decode.h','pcm_layout.h']:
  assert (SOURCE/'audio'/f).read_bytes()==(ROOT/'audio'/f).read_bytes()
 assert (SOURCE/'third_party/dr_mp3.h').read_bytes()==(ROOT/'third_party/dr_mp3.h').read_bytes()
 env={**os.environ,'SDL_VIDEODRIVER':'dummy','SDL_AUDIODRIVER':'dummy','DISPLAY':'','WAYLAND_DISPLAY':''}
 for backend in ['SCRIPTED','SDL']:
  build=OUT/backend.lower()
  run(['cmake','-S',str(SOURCE),'-B',str(build),'-DSTUDY_PLATFORM='+backend,'-DCMAKE_BUILD_TYPE=Release','-DCMAKE_CXX_FLAGS=-Wall -Wextra -Wpedantic'])
  r=run(['cmake','--build',str(build),'-j3']);(OUT/f'build-{backend}.log').write_text(r.stdout+r.stderr);assert 'warning:' not in r.stdout+r.stderr
  r=run(['ctest','--test-dir',str(build),'--output-on-failure'],env=env);(OUT/f'ctest-{backend}.log').write_text(r.stdout);print(r.stdout[-350:],flush=True)
  assert run([str(build/'mp3_probe'),str(SOURCE/'assets/rotate.mp3')]).stdout=='rate=48000 channels=2 frames=16128 samples=32256 bytes=64512 seconds=0.336\n'
 binary=OUT/'mp3-sanitized';run(['c++','-std=c++17','-O1','-fsanitize=address,undefined','-fno-sanitize-recover=all','-I'+str(SOURCE),str(SOURCE/'tests/mp3_contract.cpp'),str(SOURCE/'audio/mp3_decode.cpp'),'-o',str(binary)])
 r=run([str(binary),str(SOURCE/'assets/rotate.mp3'),str(OUT/'sanitized-files')]);(OUT/'sanitized.log').write_text(r.stdout);print(r.stdout.strip(),flush=True)
 r=run(['python3',str(ROOT/'scripts/check_learning_mp3_root.py')]);(OUT/'root-mp3.log').write_text(r.stdout);print(r.stdout.strip(),flush=True)
 build=ROOT/'out/learning-checkpoints/54-game-adapter-check/root'
 r=run(['cmake','--build',str(build),'-j3']);(OUT/'root-build.log').write_text(r.stdout+r.stderr)
 r=run(['ctest','--test-dir',str(build),'--output-on-failure'],env=env);(OUT/'root-ctest.log').write_text(r.stdout);print(r.stdout[-300:],flush=True)
 assert run([str(build/'sim_hash_dump')]).stdout==(ROOT/'python/tests/_sim_hash_dump.txt').read_text()
 print('MP3 data layer, root loader boundaries and cumulative regressions passed.',flush=True)
if __name__=='__main__':main()
