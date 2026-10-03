"""Lesson 74 PCM bounds and cumulative C++ build. No device playback claim."""
from pathlib import Path
import os
from check_learning_text_layout import run
ROOT=Path(__file__).resolve().parents[1]
SOURCE=ROOT/'docs/learn/checkpoints/74-pcm'
OUT=ROOT/'out/learning-checkpoints/74-pcm-check'
def main():
 OUT.mkdir(parents=True,exist_ok=True)
 previous=SOURCE.parent/'73-settings'
 for p in previous.rglob('*'):
  if p.is_file() and p.relative_to(previous).as_posix() not in {'CMakeLists.txt','README.md','DESIGN.md'}:
   assert p.read_bytes()==(SOURCE/p.relative_to(previous)).read_bytes(),p
 assert (SOURCE/'audio/pcm_layout.h').read_bytes()==(ROOT/'audio/pcm_layout.h').read_bytes()
 env={**os.environ,'SDL_VIDEODRIVER':'dummy','SDL_AUDIODRIVER':'dummy','DISPLAY':'','WAYLAND_DISPLAY':''}
 for backend in ['SCRIPTED','SDL']:
  build=OUT/backend.lower()
  run(['cmake','-S',str(SOURCE),'-B',str(build),'-DSTUDY_PLATFORM='+backend,'-DCMAKE_BUILD_TYPE=Release','-DCMAKE_CXX_FLAGS=-Wall -Wextra -Wpedantic'])
  r=run(['cmake','--build',str(build),'-j3']);(OUT/f'build-{backend}.log').write_text(r.stdout+r.stderr);assert 'warning:' not in r.stdout+r.stderr
  r=run(['ctest','--test-dir',str(build),'--output-on-failure'],env=env);(OUT/f'ctest-{backend}.log').write_text(r.stdout);print(r.stdout[-350:],flush=True)
  actual=run([str(build/'pcm_probe')]).stdout
  assert actual=='rate=44100 channels=2 frames=44100 samples=88200 bytes=176400 seconds=1\nlandmarks=0,6400,0,-6400,0\n'
 binary=OUT/'pcm-sanitized';run(['c++','-std=c++17','-O1','-Wall','-Wextra','-Wpedantic','-fsanitize=undefined,address','-fno-sanitize-recover=all','-I'+str(SOURCE),str(SOURCE/'tests/pcm_contract.cpp'),'-o',str(binary)])
 r=run([str(binary)]);(OUT/'sanitized.log').write_text(r.stdout);print(r.stdout.strip(),flush=True)
 # Fail the candidate's vector allocation, before the assignment body starts.
 failure=OUT/'copy-failure.cpp'
 failure.write_text(r'''#include "audio/pcm_s16.h"
#include <cstdlib>
#include <new>
#include <cstdio>
static bool fail_one=false;
void* operator new(std::size_t n){if(fail_one){fail_one=false;throw std::bad_alloc();}if(auto p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void operator delete(void* p) noexcept{std::free(p);}
void operator delete(void* p,std::size_t) noexcept{std::free(p);}
int main(){auto target=study_audio::Pcm16::make(8000,1,{7,8,9});auto source=study_audio::Pcm16::make(44100,2,{1,2,3,4,5,6,7,8});
bool threw=false;fail_one=true;try{*target=*source;}catch(const std::bad_alloc&){threw=true;}
if(!threw||fail_one||target->rate()!=8000||target->channels()!=1||target->layout().frames!=3||target->at(2,0)!=9)return 1;
std::puts("Failed PCM copy assignment preserves destination format and samples.");}
''')
 binary=OUT/'copy-failure';run(['c++','-std=c++17','-O1','-I'+str(SOURCE),str(failure),'-o',str(binary)])
 r=run([str(binary)]);(OUT/'copy-failure.log').write_text(r.stdout);print(r.stdout.strip(),flush=True)
 r=run(['python3',str(ROOT/'scripts/check_learning_pcm_root.py')]);(OUT/'root-pcm.log').write_text(r.stdout);print(r.stdout.strip(),flush=True)
 build=ROOT/'out/learning-checkpoints/54-game-adapter-check/root'
 r=run(['cmake','--build',str(build),'-j3']);(OUT/'root-build.log').write_text(r.stdout+r.stderr)
 r=run(['ctest','--test-dir',str(build),'--output-on-failure'],env=env);(OUT/'root-ctest.log').write_text(r.stdout);print(r.stdout[-300:],flush=True)
 assert run([str(build/'sim_hash_dump')]).stdout==(ROOT/'python/tests/_sim_hash_dump.txt').read_text()
 print('PCM and cumulative regressions passed.',flush=True)
if __name__=='__main__':main()
