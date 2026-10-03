"""Lesson 68 input and UI integration; manuscript review is separate."""
from pathlib import Path
import os, subprocess
from check_learning_text_layout import run
from check_learning_utf8 import cut
ROOT=Path(__file__).resolve().parents[1]
SOURCE=ROOT/'docs/learn/checkpoints/68-immediate-ui'
OUT=ROOT/'out/learning-checkpoints/68-immediate-ui-check'
def root_probe():
 head=r'''#include <cstdint>
#include <limits>
#include <cstdio>
#include <cstdlib>
static int mx=0,my=0;
static int platform_mouse_x(){return mx;}
static int platform_mouse_y(){return my;}
#define CHECK(e) do{if(!(e)){std::fprintf(stderr,"CHECK %d %s\n",__LINE__,#e);std::exit(1);}}while(false)
'''
 before=OUT/'before-gui.cpp'
 if before.exists():
  p=OUT/'root-before.cpp';p.write_text(head+cut(before.read_text(),'bool gui_hover_rect(')+r'''int main(){mx=2147483647;my=0;volatile bool result=gui_hover_rect(2147483647,0,1,1);(void)result;}''')
  run(['c++','-std=c++17','-O1','-fsanitize=undefined','-fno-sanitize-recover=all',str(p),'-o',str(OUT/'root-before')])
  result=subprocess.run([str(OUT/'root-before')],text=True,capture_output=True)
  assert result.returncode!=0 and 'signed integer overflow' in result.stderr
  (OUT/'root-before.log').write_text(result.stderr)
 source=(ROOT/'src/gui.cpp').read_text()
 p=OUT/'root-after.cpp';p.write_text(head+cut(source,'bool gui_hover_rect(')+r'''
int main(){const int values[]={(-2147483647-1),-2147483647,-100,-1,0,1,100,2147483646,2147483647};unsigned long count=0;
for(int x:values)for(int y:values)for(int w:values)for(int h:values)for(int a:values)for(int b:values){mx=a;my=b;
const bool expected=w>0&&h>0&&std::int64_t(a)-x>=0&&std::int64_t(a)-x<w&&std::int64_t(b)-y>=0&&std::int64_t(b)-y<h;
CHECK(gui_hover_rect(x,y,w,h)==expected);++count;}
std::printf("Root hover: %lu independent difference-based boundary cases passed\n",count);}
''')
 run(['c++','-std=c++17','-O1','-Wall','-Wextra','-Wpedantic','-fsanitize=undefined','-fno-sanitize-recover=all',str(p),'-o',str(OUT/'root-after')])
 result=run([str(OUT/'root-after')]);(OUT/'root-after.log').write_text(result.stdout);print(result.stdout.strip(),flush=True)
def main():
 OUT.mkdir(parents=True,exist_ok=True)
 previous=SOURCE.parent/'67-font-cache'
 allowed={'CMakeLists.txt','README.md','DESIGN.md','src/main.cpp','platform/platform.h','platform/sdl.cpp','platform/scripted.cpp','platform/win32.cpp'}
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
 binary=OUT/'ui-sanitized';run(['c++','-std=c++17','-O1','-Wall','-Wextra','-Wpedantic','-fsanitize=undefined,float-cast-overflow','-fno-sanitize-recover=all','-I'+str(SOURCE),str(SOURCE/'tests/immediate_ui_contract.cpp'),'-o',str(binary)])
 result=run([str(binary)]);(OUT/'sanitized.log').write_text(result.stdout);print(result.stdout.strip(),flush=True)
 root_probe()
 result=run(['python3',str(ROOT/'scripts/check_learning_pointer.py')]);(OUT/'root-pointer-check.log').write_text(result.stdout);print(result.stdout.strip(),flush=True)
 build=ROOT/'out/learning-checkpoints/54-game-adapter-check/root'
 result=run(['cmake','--build',str(build),'-j3']);(OUT/'root-build.log').write_text(result.stdout+result.stderr)
 result=run(['ctest','--test-dir',str(build),'--output-on-failure'],env=env);(OUT/'root-ctest.log').write_text(result.stdout);print(result.stdout[-300:],flush=True)
 assert run([str(build/'sim_hash_dump')]).stdout==(ROOT/'python/tests/_sim_hash_dump.txt').read_text()
 print('Immediate UI geometry, pointer event routing, screen transition and root hover regressions passed.',flush=True)
if __name__=='__main__':main()
