"""File decoding, owned RGBA, allocation cleanup and signed BGRA rows."""
from pathlib import Path
import os,subprocess
ROOT=Path(__file__).resolve().parents[1]
SOURCE=ROOT/'docs/learn/checkpoints/59-image-decode'
OUT=ROOT/'out/learning-checkpoints/59-image-decode-check'
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
 source=OUT/'before-image_gl.cpp' if before else ROOT/'renderer/image_gl.cpp'
 code=r"""#include <vector>
#include <memory>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <new>
#include <stdexcept>
#include "renderer/image_rows.h"
static bool reject_copy=false,fail_decode=false,bad_shape=false;
static int freed=0;static unsigned char* live=nullptr;
void* operator new(std::size_t n){if(reject_copy&&n==24){reject_copy=false;throw std::bad_alloc();}if(auto* p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void operator delete(void* p)noexcept{std::free(p);}
void operator delete(void* p,std::size_t)noexcept{std::free(p);}
#define CHECK(e) do{if(!(e)){std::fprintf(stderr,"CHECK failed %d\n",__LINE__);std::exit(1);}}while(false)
unsigned char* stbi_load(const char*,int* w,int* h,int* c,int desired){CHECK(desired==4);*w=bad_shape?0:3;*h=2;*c=3;if(fail_decode)return nullptr;live=static_cast<unsigned char*>(std::malloc(24));CHECK(live);for(int i=0;i<24;++i)live[i]=static_cast<unsigned char>(i);return live;}
void stbi_image_free(void* p){++freed;std::free(p);live=nullptr;}
const char* stbi_failure_reason(){return "test failure";}
"""+cut(source.read_text(),'static bool decode_image(')+r"""
int main(){std::vector<uint8_t> out{9,8};int w=7,h=8;reject_copy=true;
"""
 if before:
  code+=r"""bool threw=false;try{decode_image("test",out,w,h);}catch(const std::bad_alloc&){threw=true;}
 CHECK(threw&&freed==0&&live);std::free(live);std::puts("BEFORE: copy bad_alloc escaped and decoded allocation was not released");}
"""
 else:
  code+=r"""CHECK(!decode_image("test",out,w,h)&&!reject_copy&&freed==1&&!live);
 CHECK(w==7&&h==8&&out==std::vector<uint8_t>({9,8}));
 bad_shape=true;CHECK(!decode_image("test",out,w,h)&&freed==2&&w==7&&h==8);bad_shape=false;
 fail_decode=true;CHECK(!decode_image("test",out,w,h)&&out==std::vector<uint8_t>({9,8})&&w==7);fail_decode=false;
 CHECK(!decode_image(nullptr,out,w,h)&&!decode_image("",out,w,h));
 CHECK(decode_image("test",out,w,h)&&w==3&&h==2&&out.size()==24&&freed==3&&!live);
 for(int i=0;i<24;++i)CHECK(out[i]==i);
 std::puts("AFTER: copy allocation failure/shape/decode errors preserve outputs and release pixels; success commits owned bytes");}
"""
 name='before'if before else'after';p=OUT/f'root-{name}.cpp';p.write_text(code)
 flags=['c++','-std=c++17','-O1','-DNDEBUG','-fsanitize=undefined','-fno-sanitize-recover=all','-I'+str(ROOT)]
 run([*flags,str(p),'-o',str(OUT/f'root-{name}')]);result=run([str(OUT/f'root-{name}')]);print(result.stdout.strip(),flush=True)
 (OUT/f'root-{name}.log').write_text(result.stdout+result.stderr)

def lock_probe():
 code=r"""#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#define CHECK(e) do{if(!(e))std::exit(1);}while(false)
namespace Gdiplus {enum Status{Ok,GenericError};struct BitmapData{};struct Bitmap{int releases=0;bool fail=false;Status UnlockBits(BitmapData*){++releases;return fail?GenericError:Ok;}};}
"""+cut((ROOT/'renderer/image_gl.cpp').read_text(),'struct BitmapReadLock')+';\n'+r"""
int main(){Gdiplus::Bitmap b;{BitmapReadLock l(b);}CHECK(b.releases==0);
 {BitmapReadLock l(b);l.active=true;}CHECK(b.releases==1);
 {BitmapReadLock l(b);l.active=true;CHECK(l.close());CHECK(l.close());}CHECK(b.releases==2);
 try{BitmapReadLock l(b);l.active=true;throw std::runtime_error("test");}catch(...){}CHECK(b.releases==3);
 b.fail=true;{BitmapReadLock l(b);l.active=true;CHECK(!l.close());}CHECK(b.releases==4);
 std::puts("BitmapReadLock source extraction: inactive/close/unwind/error each release at most once (GDI+ test double)");}
"""
 p=OUT/'lock.cpp';p.write_text(code);run(['c++','-std=c++17',str(p),'-o',str(OUT/'lock')]);print(run([str(OUT/'lock')]).stdout.strip(),flush=True)

def main():
 OUT.mkdir(parents=True,exist_ok=True)
 previous=SOURCE.parent/'58-texture-storage'
 for p in previous.rglob('*'):
  if p.is_file() and str(p.relative_to(previous))not in {'CMakeLists.txt','README.md','src/main.cpp'}:assert p.read_bytes()==(SOURCE/p.relative_to(previous)).read_bytes(),p
 import hashlib
 assert hashlib.sha256((SOURCE/'third_party/stb_image.h').read_bytes()).hexdigest() in (SOURCE/'third_party/README.md').read_text()
 env={**os.environ,'SDL_VIDEODRIVER':'dummy','SDL_AUDIODRIVER':'dummy','DISPLAY':'','WAYLAND_DISPLAY':''}
 for backend in ['SCRIPTED','SDL']:
  build=OUT/backend.lower();run(['cmake','-S',str(SOURCE),'-B',str(build),'-DSTUDY_PLATFORM='+backend,'-DCMAKE_BUILD_TYPE=Release','-DCMAKE_CXX_FLAGS=-Wall -Wextra -Wpedantic'])
  result=run(['cmake','--build',str(build),'-j3']);(OUT/f'build-{backend}.log').write_text(result.stdout+result.stderr);assert 'warning:'not in result.stdout+result.stderr
  result=run(['ctest','--test-dir',str(build),'--output-on-failure'],env=env);(OUT/f'ctest-{backend}.log').write_text(result.stdout);print(result.stdout.strip(),flush=True)
  print(run([str(build/'decode_demo'),str(SOURCE/'assets/player.png')]).stdout.strip(),flush=True)
  if backend=='SDL':
   result=run([str(build/'decode_real'),str(SOURCE/'assets/player.png')],env={**env,'SDL_VIDEODRIVER':'offscreen','LIBGL_ALWAYS_SOFTWARE':'1'});(OUT/'pixels.log').write_text(result.stdout+result.stderr);print(result.stdout.strip(),flush=True)
 flags=['c++','-std=c++17','-O1','-DNDEBUG','-fsanitize=undefined','-fno-sanitize-recover=all','-I'+str(SOURCE),'-I'+str(SOURCE/'third_party')]
 args=[str(SOURCE/'tests/images/rgb.png')]
 run([*flags,'-include',str(SOURCE/'tests/stbi_alloc_hooks.h'),str(SOURCE/'tests/decode_allocation.cpp'),str(SOURCE/'renderer/image_decode.cpp'),'-o',str(OUT/'allocation')]);print(run([str(OUT/'allocation'),*args]).stdout.strip(),flush=True)
 run([*flags,'-I'+str(ROOT),str(ROOT/'tests/learning/current_image_rows.cpp'),'-o',str(OUT/'rows')]);print(run([str(OUT/'rows')]).stdout.strip(),flush=True)
 # The contract must detect swapped red/blue channels with all memory accesses valid.
 folder=OUT/'swapped';header=folder/'renderer/image_rows.h';header.parent.mkdir(parents=True,exist_ok=True)
 s=(ROOT/'renderer/image_rows.h').read_text();s=s.replace('dst[x + 0] = src[x + 2];','dst[x + 0] = src[x + 0];');header.write_text(s)
 run(['c++','-std=c++17','-O2','-DNDEBUG','-I'+str(folder),str(ROOT/'tests/learning/current_image_rows.cpp'),'-o',str(folder/'check')])
 result=subprocess.run([str(folder/'check')],capture_output=True,text=True);assert result.returncode==1 and 'CHECK failed'in result.stderr;(folder/'result.log').write_text(result.stderr)
 root_probe();lock_probe()
 build=ROOT/'out/learning-checkpoints/54-game-adapter-check/root';run(['cmake','-S',str(ROOT),'-B',str(build),'-DTETRIS_BUILD_REACTOR=OFF','-DCMAKE_BUILD_TYPE=Release']);run(['cmake','--build',str(build),'-j3'])
 result=run(['ctest','--test-dir',str(build),'--output-on-failure'],env=env);(OUT/'root-ctest.log').write_text(result.stdout);print(result.stdout.strip(),flush=True)
 assert run([str(build/'sim_hash_dump')]).stdout==(ROOT/'python/tests/_sim_hash_dump.txt').read_text()
 print('Image decoding, allocation failure cleanup, signed rows, GL badge, mutation and root regressions passed.',flush=True)
if __name__=='__main__':main()
