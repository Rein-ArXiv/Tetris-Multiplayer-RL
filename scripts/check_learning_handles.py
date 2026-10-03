"""Opaque handles, ownership, stale rejection and resolved image pixels."""
from pathlib import Path
import os,subprocess
ROOT=Path(__file__).resolve().parents[1]
SOURCE=ROOT/'docs/learn/checkpoints/60-image-handles'
OUT=ROOT/'out/learning-checkpoints/60-image-handles-check'
def run(args,**kw):
 result=subprocess.run(args,cwd=ROOT,text=True,capture_output=True,timeout=kw.pop('timeout',1000),**kw)
 if result.returncode:raise RuntimeError(f'{args}\n{result.stdout}\n{result.stderr}')
 return result

def cut(s,sig):
 a=s.index(sig);b=s.index('{',a);i=b+1;depth=1
 while depth:
  depth+=(s[i]=='{')-(s[i]=='}');i+=1
 return s[a:i]+'\n'

def root_probe(before=False):
 OUT.mkdir(parents=True,exist_ok=True)
 source=OUT/'before-image_gl.cpp' if before else ROOT/'renderer/image_gl.cpp';src=source.read_text()
 code=r"""#include "tests/images_fake.h"
#include "renderer/image.h"
#include <cmath>
#include <memory>
#include <new>
#include <limits>
#define max(a,b) windows_max_macro_must_not_expand
#include "renderer/handle_pool.h"
#include "renderer/texture_upload.h"
static bool fail_next_allocation=false,reject_registration=false;
void* operator new(std::size_t n){if(fail_next_allocation){fail_next_allocation=false;throw std::bad_alloc();}if(auto* p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void operator delete(void* p)noexcept{std::free(p);}
void operator delete(void* p,std::size_t)noexcept{std::free(p);}
static GLuint pending=0;static int flushes=0,draws=0;
void glb_before_texture_delete(GLuint n){if(pending==n){pending=0;++flushes;}}
void glb_rect(GLuint n,float,float,float,float,float,float,float,float,Color,float,float){CHECK(f.live.count(n));pending=n;++draws;}
void glb_quad(GLuint n,const float*,const float*,const float*,const float*,Color,float){CHECK(f.live.count(n));pending=n;++draws;}
static void STUDY_GL_CALL checked_delete(GLsizei n,const GLuint* p){CHECK(pending!=*p);del(n,p);}
static void STUDY_GL_CALL checked_upload(GLenum a,GLint b,GLint c,GLsizei d,GLsizei e,GLint f0,GLenum g,GLenum h,const void* p){upload(a,b,c,d,e,f0,g,h,p);if(reject_registration){reject_registration=false;fail_next_allocation=true;}}
decltype(gl_GetError) gl_GetError=get_error;
decltype(gl_GetIntegerv) gl_GetIntegerv=get_int;
decltype(gl_GenTextures) gl_GenTextures=gen;
decltype(gl_BindTexture) gl_BindTexture=bind;
decltype(gl_BindBuffer) gl_BindBuffer=buffer;
decltype(gl_PixelStorei) gl_PixelStorei=store;
decltype(gl_TexParameteri) gl_TexParameteri=param;
decltype(gl_TexImage2D) gl_TexImage2D=checked_upload;
decltype(gl_DeleteTextures) gl_DeleteTextures=checked_delete;
"""+cut(src,'struct ImageEntry').rstrip()+';\n'
 code+=('static std::vector<ImageEntry> s_images;\n'if before else'static image_detail::HandlePool<ImageEntry> s_images;\n')
 for signature in ['void image_init()','void image_shutdown()','ImageHandle image_create_rgba(','void image_unload(','bool image_size(','void draw_image_tinted(','void draw_image_rotated(']:code+=cut(src,signature)
 code+='int main(){unsigned char pixels[24]{};int w=99,h=88;\n'
 if before:
  code+=r"""const auto old=image_create_rgba(pixels,1,1);CHECK(old);image_unload(old);
 const auto newer=image_create_rgba(pixels,3,2);CHECK(newer&&old==newer&&image_size(old,w,h)&&w==3&&h==2);
 image_unload(old);CHECK(!image_size(newer,w,h)&&f.live.empty());
 std::puts("BEFORE: stale handle aliases replacement and can delete the new image");}
"""
 else:
  code+=r"""fail_next_allocation=true;CHECK(!image_create_rgba(pixels,1,1)&&!fail_next_allocation&&f.gens==0);
 reject_registration=true;CHECK(!image_create_rgba(pixels,1,1)&&!fail_next_allocation&&f.live.empty()&&s_images.size()==0);
 const auto old=image_create_rgba(pixels,1,1);CHECK(old&&image_size(old,w,h)&&w==1&&h==1);
 draw_image_tinted(old,0,0,20,20,WHITE);image_unload(old);CHECK(flushes==1&&f.live.empty());
 const auto newer=image_create_rgba(pixels,3,2);CHECK(newer&&newer!=old&&std::uint32_t(newer)==std::uint32_t(old));
 w=99;h=88;CHECK(!image_size(old,w,h)&&w==99&&h==88);
 auto previous=draws;draw_image_tinted(old,0,0,20,20,WHITE);draw_image_rotated(old,0,0,20,20,30);image_unload(old);
 CHECK(draws==previous&&image_size(newer,w,h)&&w==3&&f.live.size()==1);
 draw_image_rotated(newer,0,0,20,20,30);image_shutdown();CHECK(flushes==2&&f.live.empty());
 image_init();const auto restarted=image_create_rgba(pixels,1,1);CHECK(restarted&&!image_size(newer,w,h)&&!image_size(old,w,h));
 image_unload(newer);CHECK(image_size(restarted,w,h));image_shutdown();f={};
 auto good=image_create_rgba(pixels,3,2);CHECK(good);const int calls=f.call;image_shutdown();
 for(int failure=1;failure<=calls;++failure){f={};f.fail=failure;CHECK(!image_create_rgba(pixels,3,2)&&s_images.size()==0&&f.live.empty());}
 f={};f.state[MaxTextureSize]=2;CHECK(!image_create_rgba(pixels,3,2)&&f.gens==0);
 f={};CHECK(!image_create_rgba(nullptr,3,2)&&!image_create_rgba(pixels,0,2)&&f.call==0);
 image_detail::next_stamp=0;CHECK(!image_create_rgba(pixels,1,1)&&f.live.empty()&&s_images.size()==0);
 std::puts("AFTER: root stale lookup/draw/delete and reinit rejected; allocation/registration/upload/exhaustion failures clean; pending draw flushed before delete");}
"""
 name='before'if before else'after';p=OUT/f'root-{name}.cpp';code=code.replace('#include "renderer/handle_pool.h"', '#include "'+str(ROOT/'renderer/handle_pool.h')+'"');p.write_text(code)
 flags=['c++','-std=c++17','-O1','-DNDEBUG','-fsanitize=undefined','-fno-sanitize-recover=all','-I'+str(SOURCE),'-I'+str(ROOT)]
 run([*flags,str(p),'-o',str(OUT/f'root-{name}')]);result=run([str(OUT/f'root-{name}')]);(OUT/f'root-{name}.log').write_text(result.stdout+result.stderr);print(result.stdout.strip(),flush=True)

def main():
 OUT.mkdir(parents=True,exist_ok=True)
 previous=SOURCE.parent/'59-image-decode'
 for p in previous.rglob('*'):
  if p.is_file() and str(p.relative_to(previous))not in {'CMakeLists.txt','README.md','src/main.cpp'}:assert p.read_bytes()==(SOURCE/p.relative_to(previous)).read_bytes(),p
 # Stable teaching and mutable current pools are checked independently below.
 env={**os.environ,'SDL_VIDEODRIVER':'dummy','SDL_AUDIODRIVER':'dummy','DISPLAY':'','WAYLAND_DISPLAY':''}
 for backend in ['SCRIPTED','SDL']:
  build=OUT/backend.lower();run(['cmake','-S',str(SOURCE),'-B',str(build),'-DSTUDY_PLATFORM='+backend,'-DCMAKE_BUILD_TYPE=Release','-DCMAKE_CXX_FLAGS=-Wall -Wextra -Wpedantic'])
  result=run(['cmake','--build',str(build),'-j3']);(OUT/f'build-{backend}.log').write_text(result.stdout+result.stderr);assert 'warning:'not in result.stdout+result.stderr
  result=run(['ctest','--test-dir',str(build),'--output-on-failure'],env=env);(OUT/f'ctest-{backend}.log').write_text(result.stdout);print(result.stdout.strip(),flush=True)
  print(run([str(build/'handles_demo')]).stdout.strip(),flush=True)
  if backend=='SDL':
   result=run([str(build/'handles_real'),str(SOURCE/'assets/player.png')],env={**env,'SDL_VIDEODRIVER':'offscreen','LIBGL_ALWAYS_SOFTWARE':'1'});(OUT/'pixels.log').write_text(result.stdout+result.stderr);print(result.stdout.strip(),flush=True)
 flags=['c++','-std=c++17','-O1','-DNDEBUG','-fsanitize=undefined','-fno-sanitize-recover=all']
 for name,namespace,include in [('study','study_handles',SOURCE),('root','image_detail',ROOT)]:
  run([*flags,'-DPOOL_NAMESPACE='+namespace,'-I'+str(include),str(SOURCE/'tests/pool_contract.cpp'),str(SOURCE/'tests/pool_other.cpp'),'-o',str(OUT/f'pool-{name}')]);print(run([str(OUT/f'pool-{name}')]).stdout.strip(),flush=True)
 run([*flags,'-I'+str(SOURCE),'-I'+str(SOURCE/'third_party'),str(SOURCE/'tests/store_contract.cpp'),str(SOURCE/'renderer/image_decode.cpp'),'-o',str(OUT/'store-sanitized')]);print(run([str(OUT/'store-sanitized'),str(SOURCE/'assets/player.png')]).stdout.strip(),flush=True)
 folder=OUT/'ignore-stamp';header=folder/'renderer/handle_pool.h';header.parent.mkdir(parents=True,exist_ok=True)
 s=(SOURCE/'renderer/handle_pool.h').read_text();assert 'slot.stamp != stamp || 'in s;header.write_text(s.replace('slot.stamp != stamp || ',''))
 run(['c++','-std=c++17','-O2','-DNDEBUG','-I'+str(folder),str(SOURCE/'tests/pool_contract.cpp'),str(SOURCE/'tests/pool_other.cpp'),'-o',str(folder/'check')]);result=subprocess.run([str(folder/'check')],capture_output=True,text=True);assert result.returncode==1 and 'CHECK failed'in result.stderr;(folder/'result.log').write_text(result.stderr)
 root_probe()
 # Keep the earlier decode cleanup regression active on today's root source.
 from check_learning_decode import root_probe as decode_probe,lock_probe
 decode_probe();lock_probe()
 build=ROOT/'out/learning-checkpoints/54-game-adapter-check/root';run(['cmake','-S',str(ROOT),'-B',str(build),'-DTETRIS_BUILD_REACTOR=OFF','-DCMAKE_BUILD_TYPE=Release']);run(['cmake','--build',str(build),'-j3'])
 result=run(['ctest','--test-dir',str(build),'--output-on-failure'],env=env);(OUT/'root-ctest.log').write_text(result.stdout);print(result.stdout.strip(),flush=True)
 assert run([str(build/'sim_hash_dump')]).stdout==(ROOT/'python/tests/_sim_hash_dump.txt').read_text()
 print('Handle ownership, stale/foreign rejection, CPU/GL cleanup, six image frames and root regressions passed.',flush=True)
if __name__=='__main__':main()
