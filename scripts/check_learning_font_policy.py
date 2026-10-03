"""Partial lesson 67: bounded raster plans and production cache key regressions.

This is not evidence that the full cache/DPI lesson or checkpoint is complete.
"""
from pathlib import Path
import os,subprocess
from check_learning_utf8 import cut
ROOT=Path(__file__).resolve().parents[1]
SOURCE=ROOT/'docs/learn/checkpoints/67-font-cache'
OUT=ROOT/'out/learning-checkpoints/67-font-cache-check'
def run(args,**kw):
 r=subprocess.run(args,cwd=ROOT,text=True,capture_output=True,timeout=kw.pop('timeout',1000),**kw)
 if r.returncode:raise RuntimeError(f'{args}\n{r.stdout}\n{r.stderr}')
 return r

def root_probe():
 head=r'''#include "renderer/font_raster_policy.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <unordered_map>
#include <limits>
#define CHECK(e) do{if(!(e)){std::fprintf(stderr,"CHECK %d %s\n",__LINE__,#e);std::exit(1);}}while(false)
struct Glyph {int bw=0,bh=0;float w=0,h=0,xoff=0,yoff=0,advance=0,u0=0,v0=0,u1=0,v1=0;};
static constexpr float kScaleQuantum=8;static int s_font=0,s_atlas_dim=2048,calls=0;static float density=1;static bool huge_box=false;
static std::unordered_map<uint64_t,Glyph> s_cache;
static float glb_render_scale(){return density;}
static bool ensure_atlas(){return true;}
static float stbtt_ScaleForPixelHeight(const int*,float px){return px/100;}
static void stbtt_GetCodepointHMetrics(const int*,int,int* a,int* b){*a=50;*b=0;}
static void stbtt_GetCodepointBitmapBox(const int*,int,float,float,int* x0,int* y0,int* x1,int* y1){*x0=0;*y0=0;*x1=huge_box?9999:3;*y1=4;}
static unsigned char* stbtt_GetCodepointBitmap(const int*,float,float,int,int* w,int* h,int* x,int* y){++calls;*w=3;*h=4;*x=0;*y=0;return static_cast<unsigned char*>(std::calloc(12,1));}
static bool pack_glyph(const uint8_t*,int,int,Glyph& out){out.u1=out.v1=1;return true;}
static void stbtt_FreeBitmap(void* p,void*){std::free(p);}
'''
 for before in [True,False]:
  path=OUT/'before-text_gl.cpp' if before else ROOT/'renderer/text_gl.cpp'
  if not path.exists():continue
  code=head+cut(path.read_text(),'static Glyph glyph_for(')+r'''int main(){auto a=glyph_for(65,16);CHECK(a.advance==8&&calls==1);auto b=glyph_for(65,65552);'''
  if before:code+=r'''CHECK(b.advance==8&&calls==1);std::puts("Before: heights 16 and 65552 alias the same narrowed cache key; wrong advance reused");}'''
  else:code+=r'''CHECK(b.advance==32776&&b.bw==0&&calls==1&&s_cache.size()==1);
  density=std::numeric_limits<float>::infinity();b=glyph_for(66,16);CHECK(b.advance==8&&b.bw==0&&calls==1);
  density=1;huge_box=true;b=glyph_for(66,16);CHECK(b.advance==8&&b.bw==0&&calls==1&&s_cache.size()==1);
  huge_box=false;b=glyph_for(66,16);CHECK(b.bw==3&&calls==2);b=glyph_for(66,16);CHECK(calls==2);
  std::puts("After: rejected dimensions retain advance without cache alias; invalid density and oversized box rejected before bitmap allocation; retry/cache hit passed");}'''
  name='root-before' if before else 'root-after';p=OUT/(name+'.cpp');p.write_text(code)
  run(['c++','-std=c++17','-O1','-fsanitize=undefined,float-cast-overflow','-fno-sanitize-recover=all','-I'+str(ROOT),str(p),'-o',str(OUT/name)])
  r=run([str(OUT/name)]);(OUT/(name+'.log')).write_text(r.stdout);print(r.stdout.strip(),flush=True)

def main():
 OUT.mkdir(parents=True,exist_ok=True)
 assert (ROOT/'renderer/font_raster_policy.h').read_bytes()==(SOURCE/'text/raster_policy.h').read_bytes()
 run(['c++','-std=c++17','-O1','-Wall','-Wextra','-Wpedantic','-fsanitize=undefined,float-cast-overflow','-fno-sanitize-recover=all','-I'+str(SOURCE),str(SOURCE/'tests/font_policy_contract.cpp'),'-o',str(OUT/'policy-test')])
 r=run([str(OUT/'policy-test')]);(OUT/'policy-test.log').write_text(r.stdout);print(r.stdout.strip(),flush=True)
 root_probe()
 from check_learning_glyph import root_probe as bitmap_probe,reload_probe
 from check_learning_atlas import glyph_upload_probe
 from check_learning_text_layout import root_probe as measure_probe
 bitmap_probe();reload_probe();glyph_upload_probe();measure_probe()
 env={**os.environ,'SDL_VIDEODRIVER':'dummy','SDL_AUDIODRIVER':'dummy','DISPLAY':'','WAYLAND_DISPLAY':''}
 build=ROOT/'out/learning-checkpoints/54-game-adapter-check/root';r=run(['cmake','--build',str(build),'-j3']);(OUT/'root-build.log').write_text(r.stdout+r.stderr)
 r=run(['ctest','--test-dir',str(build),'--output-on-failure'],env=env);(OUT/'root-ctest.log').write_text(r.stdout);print(r.stdout[-300:],flush=True)
 assert run([str(build/'sim_hash_dump')]).stdout==(ROOT/'python/tests/_sim_hash_dump.txt').read_text()
 print('Partial lesson 67 raster policy and production regression checks passed.',flush=True)
if __name__=='__main__':main()
