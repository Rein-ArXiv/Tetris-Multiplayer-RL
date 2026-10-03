"""Texture storage, hostile unpack state, failure ownership and real badge pixels."""
from pathlib import Path
import os,subprocess
ROOT=Path(__file__).resolve().parents[1]
SOURCE=ROOT/'docs/learn/checkpoints/58-texture-storage'
OUT=ROOT/'out/learning-checkpoints/58-texture-storage-check'
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
 if not before:
  from check_learning_handles import root_probe as current_probe
  return current_probe()
 source=OUT/'before-image_gl.cpp' if before else ROOT/'renderer/image_gl.cpp'
 src=source.read_text()
 code=r"""#include "tests/texture_fake.h"
#include "renderer/image.h"
#include <limits>
#include <new>
#include <stdexcept>
// Reproduce the common Windows header macro without requiring its SDK.
#define max(a,b) windows_max_macro_must_not_expand
#include "renderer/texture_upload.h"
decltype(gl_GetError) gl_GetError=get_error;
decltype(gl_GetIntegerv) gl_GetIntegerv=get_int;
decltype(gl_GenTextures) gl_GenTextures=gen;
decltype(gl_BindTexture) gl_BindTexture=bind;
decltype(gl_BindBuffer) gl_BindBuffer=buffer;
decltype(gl_PixelStorei) gl_PixelStorei=store;
decltype(gl_TexParameteri) gl_TexParameteri=param;
decltype(gl_TexImage2D) gl_TexImage2D=upload;
decltype(gl_DeleteTextures) gl_DeleteTextures=del;
struct ImageEntry {bool used=false;int w=0,h=0;GLuint tex=0;};
static std::vector<ImageEntry> s_images;
"""+cut(src,'void image_init()')+cut(src,'ImageHandle image_create_rgba(')+r"""
int main(){unsigned char pixels[24]{};
 f.upload_error=true;auto h=image_create_rgba(pixels,3,2);
"""
 if before:
  code+=r"""CHECK(h>0&&s_images[h].used&&f.error!=0);std::puts("BEFORE: upload error still published a used image handle");}
"""
 else:
  code+=r"""CHECK(h==0&&f.live.empty()&&s_images.size()==2&&!s_images[1].used);
 f.upload_error=false;const auto old=f.state;h=image_create_rgba(pixels,3,2);
 CHECK(h==1&&s_images[1].used&&s_images[1].tex==1&&f.state==old);
 del(1,&s_images[1].tex);s_images.clear();f={};
 h=image_create_rgba(pixels,3,2);CHECK(h==1);const int calls=f.call;del(1,&s_images[1].tex);
 for(int failure=1;failure<=calls;++failure){s_images.clear();f={};f.fail=failure;
  CHECK(image_create_rgba(pixels,3,2)==0&&f.live.empty());for(const auto& e:s_images)CHECK(!e.used);
 }
 s_images.clear();f={};f.state[MaxTextureSize]=2;CHECK(image_create_rgba(pixels,3,2)==0&&f.gens==0);
 s_images.clear();f={};CHECK(image_create_rgba(nullptr,3,2)==0&&image_create_rgba(pixels,0,2)==0&&f.call==0);
 std::puts("AFTER: root image handle published only after upload/restoration; failure slots reusable; all GL operation failures clean");}
"""
 name='before'if before else'after';p=OUT/f'root-{name}.cpp';p.write_text(code)
 run(['c++','-std=c++17','-O1','-DNDEBUG','-fsanitize=undefined','-fno-sanitize-recover=all','-I'+str(SOURCE),'-I'+str(ROOT),str(p),'-o',str(OUT/f'root-{name}')])
 result=run([str(OUT/f'root-{name}')]);(OUT/f'root-{name}.log').write_text(result.stdout);print(result.stdout.strip(),flush=True)

def main():
 OUT.mkdir(parents=True,exist_ok=True)
 previous=SOURCE.parent/'57-flush-boundaries'
 allowed={'CMakeLists.txt','README.md','src/main.cpp','renderer/gl_api.h','renderer/gl_api.cpp','tests/loader_contract.cpp'}
 for p in previous.rglob('*'):
  if p.is_file() and str(p.relative_to(previous))not in allowed:assert p.read_bytes()==(SOURCE/p.relative_to(previous)).read_bytes(),p
 env={**os.environ,'SDL_VIDEODRIVER':'dummy','SDL_AUDIODRIVER':'dummy','DISPLAY':'','WAYLAND_DISPLAY':''}
 for backend in ['SCRIPTED','SDL']:
  build=OUT/backend.lower();run(['cmake','-S',str(SOURCE),'-B',str(build),'-DSTUDY_PLATFORM='+backend,'-DCMAKE_BUILD_TYPE=Release','-DCMAKE_CXX_FLAGS=-Wall -Wextra -Wpedantic'])
  result=run(['cmake','--build',str(build),'-j3']);(OUT/f'build-{backend}.log').write_text(result.stdout+result.stderr);assert 'warning:'not in result.stdout+result.stderr
  result=run(['ctest','--test-dir',str(build),'--output-on-failure'],env=env);(OUT/f'ctest-{backend}.log').write_text(result.stdout);print(result.stdout.strip(),flush=True)
  print(run([str(build/'texture_demo')]).stdout.strip(),flush=True)
  if backend=='SDL':
   result=run([str(build/'texture_real')],env={**env,'SDL_VIDEODRIVER':'offscreen','LIBGL_ALWAYS_SOFTWARE':'1'});(OUT/'pixels.log').write_text(result.stdout+result.stderr);print(result.stdout.strip(),flush=True)
 flags=['c++','-std=c++17','-O1','-DNDEBUG','-fsanitize=undefined','-fno-sanitize-recover=all','-I'+str(SOURCE)]
 run([*flags,str(SOURCE/'tests/texture_contract.cpp'),'-o',str(OUT/'texture-sanitized')]);print(run([str(OUT/'texture-sanitized')]).stdout.strip(),flush=True)
 run([*flags,str(SOURCE/'tests/texture_quad_contract.cpp'),str(SOURCE/'renderer/program.cpp'),str(SOURCE/'renderer/shader.cpp'),'-o',str(OUT/'quad-sanitized')]);print(run([str(OUT/'quad-sanitized')]).stdout.strip(),flush=True)
 for name,before,after in [('skip_rows','gl_.PixelStorei(UnpackSkipRows,0);','/* omitted */'),('publish_failure','if (!ok || !restored) {','if (!restored) {'),('leak_failure','if (candidate) gl_.DeleteTextures(1,&candidate);','/* omitted */')]:
  folder=OUT/name;p=folder/'renderer/texture.h';p.parent.mkdir(parents=True,exist_ok=True);text=(SOURCE/'renderer/texture.h').read_text();assert before in text;p.write_text(text.replace(before,after))
  run(['c++','-std=c++17','-O2','-DNDEBUG','-I'+str(folder),'-I'+str(SOURCE),str(SOURCE/'tests/texture_contract.cpp'),'-o',str(folder/'check')])
  result=subprocess.run([str(folder/'check')],capture_output=True,text=True);assert result.returncode==1 and 'CHECK failed'in result.stderr,name;(folder/'result.log').write_text(result.stderr)
 root_probe()
 build=ROOT/'out/learning-checkpoints/54-game-adapter-check/root';run(['cmake','-S',str(ROOT),'-B',str(build),'-DTETRIS_BUILD_REACTOR=OFF','-DCMAKE_BUILD_TYPE=Release']);run(['cmake','--build',str(build),'-j3'])
 result=run(['ctest','--test-dir',str(build),'--output-on-failure'],env=env);(OUT/'root-ctest.log').write_text(result.stdout);print(result.stdout.strip(),flush=True)
 assert run([str(build/'sim_hash_dump')]).stdout==(ROOT/'python/tests/_sim_hash_dump.txt').read_text()
 print('Texture bytes, unpack restoration, lifetime/errors, badge pixels, UBSan, three mutations and root regressions passed.',flush=True)
if __name__=='__main__':main()
