"""Shelf placement, R8 atlas upload ownership, bilinear pixels and root reuse regressions."""
from pathlib import Path
import os, subprocess
from check_learning_utf8 import cut
from check_learning_glyph import root_probe as glyph_probe, reload_probe
ROOT=Path(__file__).resolve().parents[1]
SOURCE=ROOT/'docs/learn/checkpoints/65-atlas'
OUT=ROOT/'out/learning-checkpoints/65-atlas-check'
def run(args,**kw):
 r=subprocess.run(args,cwd=ROOT,text=True,capture_output=True,timeout=kw.pop('timeout',1000),**kw)
 if r.returncode:raise RuntimeError(f'{args}\n{r.stdout}\n{r.stderr}')
 return r

def root_probe():
 head=r'''#include "tests/atlas_fake.h"
 #include "renderer/mask_upload.h"
 #include <algorithm>
 #include <cstdint>
 #include <unordered_map>
 #define DEFINE_GL(ret,name,args) ret(TETRIS_GL_APIENTRY* gl_##name)args=nullptr;
 GL_FUNCS(DEFINE_GL)
 #undef DEFINE_GL
 struct Glyph {int bw=0,bh=0;float u0=0,v0=0,u1=0,v1=0;};
 static GLuint s_atlas=1;static int s_atlas_dim=16,s_pen_x=0,s_pen_y=0,s_row_h=0;
 static std::unordered_map<unsigned,Glyph> s_cache;
 static int flushed=0;
 static void glb_flush(){CHECK(f.textures.at(s_atlas).bytes[0]==255);++flushed;}
 static void setup(){gl_GetError=get_error;gl_GetIntegerv=get_int;gl_GenTextures=gen;gl_BindTexture=bind;gl_BindBuffer=buffer;gl_PixelStorei=store;gl_TexParameteri=param;gl_TexImage2D=upload;gl_TexSubImage2D=sub;gl_DeleteTextures=del;}
 '''
 code=head+cut((ROOT/'renderer/text_gl.cpp').read_text(),'static bool pack_glyph(')
 code+=r'''int main(){setup();f={};f.live.insert(1);f.textures[1]={16,16,std::vector<unsigned char>(256,255)};f.state[TextureBinding2D]=0;f.state[PixelUnpackBufferBinding]=0;f.state[UnpackAlignment]=1;f.state[UnpackRowLength]=0;f.state[UnpackSkipRows]=0;f.state[UnpackSkipPixels]=0;s_pen_y=16;s_cache.emplace(1,Glyph{});const unsigned char pixels[4]={80,80,80,80};Glyph result;CHECK(pack_glyph(pixels,2,2,result));CHECK(flushed==1&&s_cache.empty());
'''
 code+=r'''CHECK(result.u0==1.f/16&&result.v0==1.f/16);for(int y=0;y<4;++y)for(int x=0;x<4;++x)CHECK(f.textures[1].bytes[y*16+x]==((x==1||x==2)&&(y==1||y==2)?80:0));
 const int px=s_pen_x,py=s_pen_y,rh=s_row_h;f.upload_error=true;CHECK(!pack_glyph(pixels,2,2,result));CHECK(s_pen_x==px&&s_pen_y==py&&s_row_h==rh);f.upload_error=false;CHECK(pack_glyph(pixels,2,2,result));std::puts("AFTER: all four borders overwritten with zero, old UVs submitted, failed upload preserves placement");}'''
 p=OUT/'root-after.cpp';p.write_text(code)
 run(['c++','-std=c++17','-O1','-fsanitize=undefined','-fno-sanitize-recover=all','-I'+str(SOURCE),'-I'+str(ROOT),str(p),'-o',str(OUT/'root-after')])
 result=run([str(OUT/'root-after')]);(OUT/'root-after.log').write_text(result.stdout);print(result.stdout.strip(),flush=True)

 # Actual root helper, with unrelated unpack state and one injected GL error at each call.
 code=head+r'''int main(){setup();int init_calls=0,write_calls=0;
 f={};{const auto old=f.state;auto t=text_detail::create_mask8(16);CHECK(t&&f.state==old);init_calls=f.call;CHECK(std::all_of(f.textures[t].bytes.begin(),f.textures[t].bytes.end(),[](auto x){return x==0;}));
 const unsigned char pixels[6]={1,2,3,4,5,6};const int start=f.call;CHECK(text_detail::update_mask8(t,2,3,3,2,pixels));write_calls=f.call-start;CHECK(f.state==old);CHECK(f.textures[t].bytes[3*16+2]==1&&f.textures[t].bytes[4*16+4]==6);gl_DeleteTextures(1,&t);CHECK(f.live.empty());}
 for(int fail=1;fail<=init_calls;++fail){f={};f.fail=fail;CHECK(!text_detail::create_mask8(16));CHECK(f.live.empty());}
 for(int fail=1;fail<=write_calls;++fail){f={};auto t=text_detail::create_mask8(16);CHECK(t);const unsigned char pixels[6]={};f.fail=f.call+fail;CHECK(!text_detail::update_mask8(t,2,3,3,2,pixels));f.fail=0;CHECK(text_detail::update_mask8(t,2,3,3,2,pixels));gl_DeleteTextures(1,&t);CHECK(f.live.empty());}
 std::printf("Root R8 helper: %d allocation / %d update fault stages; zero initial pixels, unpack restore and retry passed\n",init_calls,write_calls);}
'''
 p=OUT/'root-helper.cpp';p.write_text(code)
 run(['c++','-std=c++17','-O1','-fsanitize=undefined','-fno-sanitize-recover=all','-I'+str(SOURCE),'-I'+str(ROOT),str(p),'-o',str(OUT/'root-helper')])
 r=run([str(OUT/'root-helper')]);(OUT/'root-helper.log').write_text(r.stdout);print(r.stdout.strip(),flush=True)

def glyph_upload_probe():
 head=r'''#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <unordered_map>
#define CHECK(e) do{if(!(e)){std::fprintf(stderr,"CHECK failed %d\\n",__LINE__);std::exit(1);}}while(false)
struct Glyph {int bw=0,bh=0;float w=0,h=0,xoff=0,yoff=0,advance=0,u0=0,v0=0,u1=0,v1=0;};
static constexpr float kScaleQuantum=8;static int s_atlas_dim=2048;static int s_font=0,calls=0,frees=0;static bool atlas_ok=false,pack_ok=false;
static std::unordered_map<uint64_t,Glyph> s_cache;
#include "renderer/font_raster_policy.h"
static void stbtt_GetCodepointBitmapBox(const int*,int,float,float,int* x0,int* y0,int* x1,int* y1){*x0=1;*y0=-3;*x1=4;*y1=1;}
static float glb_render_scale(){return 1;}
static bool ensure_atlas(){return atlas_ok;}
static float stbtt_ScaleForPixelHeight(const int*,float px){return px/100;}
static void stbtt_GetCodepointHMetrics(const int*,int,int* advance,int* bearing){*advance=50;*bearing=0;}
static unsigned char* stbtt_GetCodepointBitmap(const int*,float,float,int,int* w,int* h,int* x,int* y){++calls;*w=3;*h=4;*x=1;*y=-3;return static_cast<unsigned char*>(std::calloc(12,1));}
static bool pack_glyph(const uint8_t*,int,int,Glyph& out){if(!pack_ok)return false;out.u1=out.v1=1;return true;}
static void stbtt_FreeBitmap(void* p,void*){++frees;std::free(p);}
'''
 code=head+cut((ROOT/'renderer/text_gl.cpp').read_text(),'static Glyph glyph_for(')+r'''
int main(){auto g=glyph_for(65,20);CHECK(g.advance==10&&g.bw==0&&calls==0&&s_cache.empty());
atlas_ok=true;g=glyph_for(65,20);CHECK(g.advance==10&&g.bw==0&&calls==1&&frees==1&&s_cache.empty());
pack_ok=true;g=glyph_for(65,20);CHECK(g.advance==10&&g.bw==3&&calls==2&&frees==2&&s_cache.size()==1&&g.u1==1);
g=glyph_for(65,20);CHECK(calls==2);std::puts("Root glyph allocation/upload failure: advance retained, bitmap freed, cache omitted, retry and cache hit passed");}
'''
 p=OUT/'root-glyph-upload.cpp';p.write_text(code)
 run(['c++','-std=c++17','-O1','-fsanitize=undefined','-fno-sanitize-recover=all','-I'+str(ROOT),str(p),'-o',str(OUT/'root-glyph-upload')])
 result=run([str(OUT/'root-glyph-upload')]);(OUT/'root-glyph-upload.log').write_text(result.stdout);print(result.stdout.strip(),flush=True)

def main():
 OUT.mkdir(parents=True,exist_ok=True)
 previous=SOURCE.parent/'64-glyph'
 allowed={'CMakeLists.txt','README.md','src/main.cpp','renderer/gl_api.h','renderer/gl_api.cpp','renderer/image_quad.h','tests/loader_contract.cpp'}
 for p in previous.rglob('*'):
  if p.is_file() and p.relative_to(previous).as_posix() not in allowed:
   assert p.read_bytes()==(SOURCE/p.relative_to(previous)).read_bytes(),p
 env={**os.environ,'SDL_VIDEODRIVER':'dummy','SDL_AUDIODRIVER':'dummy','DISPLAY':'','WAYLAND_DISPLAY':''}
 for backend in ['SCRIPTED','SDL']:
  build=OUT/backend.lower();run(['cmake','-S',str(SOURCE),'-B',str(build),'-DSTUDY_PLATFORM='+backend,'-DCMAKE_BUILD_TYPE=Release','-DCMAKE_CXX_FLAGS=-Wall -Wextra -Wpedantic'])
  r=run(['cmake','--build',str(build),'-j3']);(OUT/f'build-{backend}.log').write_text(r.stdout+r.stderr);assert 'warning:' not in r.stdout+r.stderr
  r=run(['ctest','--test-dir',str(build),'--output-on-failure'],env=env);(OUT/f'ctest-{backend}.log').write_text(r.stdout);print(r.stdout[-350:],flush=True)
 r=run([str(OUT/'sdl/atlas_real'),str(SOURCE/'assets/NanumGothic.ttf')],env={**env,'SDL_VIDEODRIVER':'offscreen','LIBGL_ALWAYS_SOFTWARE':'1'});(OUT/'pixels.log').write_text(r.stdout+r.stderr);print(r.stdout.strip(),flush=True)
 r=run([str(OUT/'scripted/atlas_demo')]);(OUT/'demo.log').write_text(r.stdout)
 for name in ['shelf','atlas']:
  run(['c++','-std=c++17','-O1','-fsanitize=undefined','-fno-sanitize-recover=all','-I'+str(SOURCE),str(SOURCE/f'tests/{name}_contract.cpp'),'-o',str(OUT/f'{name}-sanitized')])
  r=run([str(OUT/f'{name}-sanitized')]);(OUT/f'{name}-sanitized.log').write_text(r.stdout);print(r.stdout.strip(),flush=True)
 root_probe();glyph_upload_probe();glyph_probe();reload_probe()
 build=ROOT/'out/learning-checkpoints/54-game-adapter-check/root';r=run(['cmake','--build',str(build),'-j3']);(OUT/'root-build.log').write_text(r.stdout+r.stderr)
 r=run(['ctest','--test-dir',str(build),'--output-on-failure'],env=env);(OUT/'root-ctest.log').write_text(r.stdout);print(r.stdout[-300:],flush=True)
 assert run([str(build/'sim_hash_dump')]).stdout==(ROOT/'python/tests/_sim_hash_dump.txt').read_text()
 print('Atlas packing, failure contracts, pixels, root reuse and game regressions passed.',flush=True)
if __name__=='__main__':main()
