"""Text layout metrics, kern table, real pixels and CPU-only root measurement."""
from pathlib import Path
import os,subprocess
from check_learning_utf8 import cut
ROOT=Path(__file__).resolve().parents[1]
SOURCE=ROOT/'docs/learn/checkpoints/66-text-layout'
OUT=ROOT/'out/learning-checkpoints/66-text-layout-check'
def run(args,**kw):
 r=subprocess.run(args,cwd=ROOT,text=True,capture_output=True,timeout=kw.pop('timeout',1000),**kw)
 if r.returncode:raise RuntimeError(f'{args}\n{r.stdout}\n{r.stderr}')
 return r

def root_probe():
 head=r'''#include "core/utf8.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <vector>
#include <array>
#define CHECK(e) do{if(!(e)){std::fprintf(stderr,"CHECK %d %s\n",__LINE__,#e);std::exit(1);}}while(false)
struct Color{unsigned char a=255;};
struct Glyph{int bw=2,bh=3;float w=2,h=3,xoff=-1,yoff=-8,advance=0,u0=0,v0=0,u1=1,v1=1;};
static bool s_font_ok=true;static int s_font=0,s_atlas=1,gpu_calls=0;
static std::vector<std::array<float,2>> positions;
static float stbtt_ScaleForPixelHeight(const int*,float px){return px/10;}
static int units(int cp){return cp=='A'?10:cp=='V'?8:cp==' '?3:5;}
static void stbtt_GetCodepointHMetrics(const int*,int cp,int* a,int* b){*a=units(cp);*b=0;}
static int stbtt_GetCodepointKernAdvance(const int*,int a,int b){return a=='A'&&b=='V'?-2:0;}
static void stbtt_GetFontVMetrics(const int*,int* a,int* d,int* g){*a=8;*d=-2;*g=3;}
static Glyph glyph_for(uint32_t cp,int px){++gpu_calls;Glyph g;g.advance=units(int(cp))*px/10.f;return g;}
static void glb_rect(int,float x,float y,float,float,float,float,float,float,Color,float,float){positions.push_back({x,y});}
'''
 source=(ROOT/'renderer/text_gl.cpp').read_text()
 code=head+cut(source,'static uint32_t utf8_next(')+cut(source,'int measure_text(')+cut(source,'void draw_text(')+r'''
int main(){CHECK(measure_text(nullptr,10)==0&&measure_text("",10)==0);CHECK(measure_text("AV\nA V\n",10)==21);CHECK(gpu_calls==0);
 draw_text("AV\nA V\n",10,20,10,{});CHECK(positions.size()==5&&positions[0][0]==9&&positions[1][0]==17&&positions[2][0]==9&&positions[2][1]==33&&positions[4][0]==22);
 const int old=gpu_calls;CHECK(measure_text("AV",10)==16&&gpu_calls==old);CHECK(measure_text("AA",(std::numeric_limits<int>::max)())==(std::numeric_limits<int>::max)());
 CHECK(measure_text("A",0)==1);s_font_ok=false;CHECK(measure_text("A",10)==0);
 std::puts("Root measure/draw: matching kern/newline pens, CPU-only measurement, null/font guards and int saturation passed");}
'''
 p=OUT/'root-measure.cpp';p.write_text(code)
 run(['c++','-std=c++17','-O1','-fsanitize=undefined,float-cast-overflow','-fno-sanitize-recover=all','-I'+str(ROOT),str(p),'-o',str(OUT/'root-measure')]);r=run([str(OUT/'root-measure')]);(OUT/'root-measure.log').write_text(r.stdout);print(r.stdout.strip(),flush=True)
 baseline=OUT/'before-text_gl.cpp'
 if baseline.exists():
  code=head+cut(source,'static uint32_t utf8_next(')+cut(baseline.read_text(),'int measure_text(')+r'''int main(){CHECK(measure_text("AV\nA V\n",10)==21&&gpu_calls==5);std::puts("Before: width-only request reaches glyph/GPU preparation for all five non-newline scalars");}'''
  p=OUT/'root-before.cpp';p.write_text(code);run(['c++','-std=c++17','-I'+str(ROOT),str(p),'-o',str(OUT/'root-before')]);r=run([str(OUT/'root-before')]);(OUT/'root-before.log').write_text(r.stdout)

def main():
 OUT.mkdir(parents=True,exist_ok=True)
 previous=SOURCE.parent/'65-atlas';allowed={'CMakeLists.txt','README.md','src/main.cpp','text/font.h','text/font.cpp'}
 for p in previous.rglob('*'):
  if p.is_file() and p.relative_to(previous).as_posix() not in allowed:
   assert p.read_bytes()==(SOURCE/p.relative_to(previous)).read_bytes(),p
 env={**os.environ,'SDL_VIDEODRIVER':'dummy','SDL_AUDIODRIVER':'dummy','DISPLAY':'','WAYLAND_DISPLAY':''}
 for backend in ['SCRIPTED','SDL']:
  build=OUT/backend.lower();run(['cmake','-S',str(SOURCE),'-B',str(build),'-DSTUDY_PLATFORM='+backend,'-DCMAKE_BUILD_TYPE=Release','-DCMAKE_CXX_FLAGS=-Wall -Wextra -Wpedantic'])
  r=run(['cmake','--build',str(build),'-j3']);(OUT/f'build-{backend}.log').write_text(r.stdout+r.stderr);assert 'warning:' not in r.stdout+r.stderr
  r=run(['ctest','--test-dir',str(build),'--output-on-failure'],env=env);(OUT/f'ctest-{backend}.log').write_text(r.stdout);print(r.stdout[-350:],flush=True)
 r=run([str(OUT/'sdl/layout_real'),str(SOURCE/'assets/NanumGothic.ttf')],env={**env,'SDL_VIDEODRIVER':'offscreen','LIBGL_ALWAYS_SOFTWARE':'1'});(OUT/'pixels.log').write_text(r.stdout+r.stderr);print(r.stdout.strip(),flush=True)
 r=run([str(OUT/'sdl/text_layout_demo'),str(SOURCE/'assets/NanumGothic.ttf')]);(OUT/'demo.log').write_text(r.stdout);print(r.stdout.strip(),flush=True)
 run(['c++','-std=c++17','-O1','-fsanitize=undefined,float-cast-overflow','-fno-sanitize-recover=all','-I'+str(SOURCE),'-isystem',str(SOURCE/'third_party'),str(SOURCE/'text/font.cpp'),str(SOURCE/'tests/layout_contract.cpp'),'-o',str(OUT/'layout-sanitized')])
 r=run([str(OUT/'layout-sanitized'),str(SOURCE/'assets/NanumGothic.ttf')]);(OUT/'sanitized.log').write_text(r.stdout);print(r.stdout.strip(),flush=True)
 root_probe()
 build=ROOT/'out/learning-checkpoints/54-game-adapter-check/root';r=run(['cmake','--build',str(build),'-j3']);(OUT/'root-build.log').write_text(r.stdout+r.stderr)
 r=run(['ctest','--test-dir',str(build),'--output-on-failure'],env=env);(OUT/'root-ctest.log').write_text(r.stdout);print(r.stdout[-300:],flush=True)
 assert run([str(build/'sim_hash_dump')]).stdout==(ROOT/'python/tests/_sim_hash_dump.txt').read_text()
 print('Text placement, kerning, metrics, real pixels and root measurement regressions passed.',flush=True)
if __name__=='__main__':main()
