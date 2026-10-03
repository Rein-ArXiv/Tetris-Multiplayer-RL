"""Font metrics/ownership, glyph texture lifetime and CPU-to-GPU coverage."""
from pathlib import Path
import os,subprocess
from check_learning_utf8 import cut
ROOT=Path(__file__).resolve().parents[1]
SOURCE=ROOT/'docs/learn/checkpoints/64-glyph'
OUT=ROOT/'out/learning-checkpoints/64-glyph-check'
def run(args,**kw):
 r=subprocess.run(args,cwd=ROOT,text=True,capture_output=True,timeout=kw.pop('timeout',1000),**kw)
 if r.returncode:raise RuntimeError(f'{args}\n{r.stdout}\n{r.stderr}')
 return r

def root_probe():
 head=r'''#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <unordered_map>
#define CHECK(e) do{if(!(e)){std::fprintf(stderr,"CHECK failed %d\n",__LINE__);std::exit(1);}}while(false)
struct Glyph {int bw=0,bh=0;float w=0,h=0,xoff=0,yoff=0,advance=0,u0=0,v0=0,u1=0,v1=0;};
static constexpr float kScaleQuantum=8;static int s_atlas_dim=2048;static int s_font=0,calls=0;static bool fail=true;
static std::unordered_map<uint64_t,Glyph> s_cache;
#include "renderer/font_raster_policy.h"
static void stbtt_GetCodepointBitmapBox(const int*,int,float,float,int* x0,int* y0,int* x1,int* y1){*x0=1;*y0=-3;*x1=4;*y1=1;}
static float glb_render_scale(){return 1;}
static bool ensure_atlas(){return true;}
static float stbtt_ScaleForPixelHeight(const int*,float px){return px/100;}
static void stbtt_GetCodepointHMetrics(const int*,int,int* advance,int* bearing){*advance=50;*bearing=0;}
static unsigned char* stbtt_GetCodepointBitmap(const int*,float,float,int,int* w,int* h,int* x,int* y){++calls;*w=3;*h=4;*x=1;*y=-3;return fail?nullptr:static_cast<unsigned char*>(std::calloc(12,1));}
static bool pack_glyph(const uint8_t*,int,int,Glyph& out){out.u1=out.v1=1;return true;}
static void stbtt_FreeBitmap(void* p,void*){std::free(p);}
'''
 code=head+cut((ROOT/'renderer/text_gl.cpp').read_text(),'static Glyph glyph_for(')+r'''
int main(){auto g=glyph_for(65,20);CHECK(g.bw==0&&g.bh==0&&g.advance==10&&s_cache.empty());fail=false;g=glyph_for(65,20);CHECK(calls==2&&g.bw==3&&g.u1==1&&s_cache.size()==1);g=glyph_for(65,20);CHECK(calls==2);std::puts("Failed bitmap: spacing retained, no draw/cache, retry succeeds");}
'''
 p=OUT/'root-after.cpp';p.write_text(code);run(['c++','-std=c++17','-O1','-fsanitize=undefined','-fno-sanitize-recover=all','-I'+str(ROOT),str(p),'-o',str(OUT/'root-after')]);r=run([str(OUT/'root-after')]);(OUT/'root-after.log').write_text(r.stdout)

def reload_probe():
 head=r'''#include <cstdio>
 #include <cstdlib>
 #include <cstdint>
 #include <vector>
 #define CHECK(e) do{if(!(e))std::exit(1);}while(false)
 static bool s_font_ok=true;static std::vector<int> s_cache{1};static std::vector<uint8_t> s_ttf{1};static int s_font=0,s_pen_x=7,s_pen_y=8,s_row_h=9,flushed=0;
 static void glb_flush(){CHECK(s_font_ok&&s_cache.size()==1&&s_ttf.size()==1&&s_pen_x==7);++flushed;}
 static int stbtt_GetFontOffsetForIndex(const uint8_t*,int){return 0;}
 static int stbtt_InitFont(int*,const uint8_t*,int){return 1;}
 '''
 code=head+cut((ROOT/'renderer/text_gl.cpp').read_text(),'bool renderer_load_font(')+r'''int main(){CHECK(!renderer_load_font(nullptr));CHECK(!s_font_ok&&s_cache.empty()&&s_ttf.empty()&&s_pen_x==0);CHECK(flushed==1);std::puts("Font replacement submits old glyph references before reset");}'''
 p=OUT/'reload-after.cpp';p.write_text(code);run(['c++','-std=c++17','-O1','-fsanitize=undefined','-fno-sanitize-recover=all','-I'+str(ROOT),str(p),'-o',str(OUT/'reload-after')]);r=run([str(OUT/'reload-after')]);(OUT/'reload-after.log').write_text(r.stdout)

def main():
 OUT.mkdir(parents=True,exist_ok=True)
 for p in (SOURCE.parent/'63-utf8').rglob('*'):
  if p.is_file() and str(p.relative_to(SOURCE.parent/'63-utf8')) not in {'CMakeLists.txt','README.md','src/main.cpp','third_party/README.md'}:
   assert p.read_bytes()==(SOURCE/p.relative_to(SOURCE.parent/'63-utf8')).read_bytes(),p
 assert (SOURCE/'third_party/stb_truetype.h').read_bytes()==(ROOT/'third_party/stb_truetype.h').read_bytes()
 assert (SOURCE/'assets/NanumGothic.ttf').read_bytes()==(ROOT/'Font/NanumGothic.ttf').read_bytes()
 env={**os.environ,'SDL_VIDEODRIVER':'dummy','SDL_AUDIODRIVER':'dummy','DISPLAY':'','WAYLAND_DISPLAY':''}
 for backend in ['SCRIPTED','SDL']:
  build=OUT/backend.lower();run(['cmake','-S',str(SOURCE),'-B',str(build),'-DSTUDY_PLATFORM='+backend,'-DCMAKE_BUILD_TYPE=Release','-DCMAKE_CXX_FLAGS=-Wall -Wextra -Wpedantic'])
  r=run(['cmake','--build',str(build),'-j3']);(OUT/f'build-{backend}.log').write_text(r.stdout+r.stderr);assert 'warning:' not in r.stdout+r.stderr
  r=run(['ctest','--test-dir',str(build),'--output-on-failure'],env=env);(OUT/f'ctest-{backend}.log').write_text(r.stdout);print(r.stdout[-350:],flush=True)
 r=run([str(OUT/'sdl/glyph_real'),str(SOURCE/'assets/NanumGothic.ttf')],env={**env,'SDL_VIDEODRIVER':'offscreen','LIBGL_ALWAYS_SOFTWARE':'1'});(OUT/'pixels.log').write_text(r.stdout+r.stderr);print(r.stdout.strip(),flush=True)
 flags=['c++','-std=c++17','-O1','-fsanitize=undefined','-fno-sanitize-recover=all','-I'+str(SOURCE),'-isystem',str(SOURCE/'third_party')]
 run([*flags,str(SOURCE/'text/font.cpp'),str(SOURCE/'tests/glyph_contract.cpp'),'-o',str(OUT/'glyph-sanitized')]);r=run([str(OUT/'glyph-sanitized'),str(SOURCE/'assets/NanumGothic.ttf')]);(OUT/'sanitized.log').write_text(r.stdout);print(r.stdout.strip(),flush=True)
 run([*flags,str(SOURCE/'tests/text_line_contract.cpp'),'-o',str(OUT/'text-line-sanitized')]);r=run([str(OUT/'text-line-sanitized')]);(OUT/'text-line-sanitized.log').write_text(r.stdout);print(r.stdout.strip(),flush=True)
 root_probe();reload_probe()
 build=ROOT/'out/learning-checkpoints/54-game-adapter-check/root';run(['cmake','--build',str(build),'-j3']);r=run(['ctest','--test-dir',str(build),'--output-on-failure'],env=env);(OUT/'root-ctest.log').write_text(r.stdout);print(r.stdout[-300:],flush=True)
 assert run([str(build/'sim_hash_dump')]).stdout==(ROOT/'python/tests/_sim_hash_dump.txt').read_text()
 print('Glyph metrics, lifetime, pixels, root bitmap failure and game regressions passed.',flush=True)
if __name__=='__main__':main()
