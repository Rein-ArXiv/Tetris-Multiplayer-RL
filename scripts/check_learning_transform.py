"""Image transform, UV/tint vertices, blend pixels and bounded rotation."""
from pathlib import Path
import os,subprocess
ROOT=Path(__file__).resolve().parents[1]
SOURCE=ROOT/'docs/learn/checkpoints/61-image-transform'
OUT=ROOT/'out/learning-checkpoints/61-image-transform-check'
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
 source=OUT/'before-image_gl.cpp' if before else ROOT/'renderer/image_gl.cpp'
 code=r"""#include "renderer/image.h"
#include <cmath>
#include <limits>
#include <cstdio>
#include <cstdlib>
using GLuint=unsigned;
struct ImageEntry{GLuint tex=1;};
struct Entries{ImageEntry e;const ImageEntry* find(ImageHandle h)const{return h==1?&e:nullptr;}}s_images;
static int calls=0;static float x[4]{},y[4]{};
void glb_quad(GLuint,const float* px,const float* py,const float*,const float*,Color,float){++calls;for(int i=0;i<4;++i){x[i]=px[i];y[i]=py[i];}}
#define CHECK(e) do{if(!(e)){std::fprintf(stderr,"CHECK failed %d\n",__LINE__);std::exit(1);}}while(false)
"""+cut(source.read_text(),'void draw_image_rotated(')+r"""
int main(){draw_image_rotated(1,10,20,20,10,(std::numeric_limits<float>::max)());
"""
 if before:code+=r"""CHECK(calls==1&&(!std::isfinite(x[0])||!std::isfinite(y[0])));std::puts("BEFORE: finite FLOAT_MAX angle overflows radians and queues nonfinite coordinates");}
"""
 else:code+=r"""CHECK(calls==1);for(int i=0;i<4;++i)CHECK(std::isfinite(x[i])&&std::isfinite(y[i]));
 for(float a:{std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::infinity(),-std::numeric_limits<float>::infinity()}){calls=0;draw_image_rotated(1,10,20,20,10,a);CHECK(calls==0);}
 calls=0;draw_image_rotated(0,10,20,20,10,90);draw_image_rotated(1,10,20,0,10,90);CHECK(calls==0);
 draw_image_rotated(1,10,20,20,10,90);CHECK(calls==1&&std::abs(x[0]-15)<.0001&&std::abs(y[0]-10)<.0001);
 float savedx=x[0],savedy=y[0];draw_image_rotated(1,10,20,20,10,810);CHECK(x[0]==savedx&&y[0]==savedy);
 std::puts("AFTER: huge finite rotation stays finite, NaN/Inf rejected before queue, 90 degree corner and whole-turn equivalence passed");}
"""
 code=code.replace('#include <cmath>','#include <cmath>\n#include <initializer_list>')
 name='before'if before else'after';p=OUT/f'root-{name}.cpp';p.write_text(code)
 run(['c++','-std=c++17','-O1','-DNDEBUG','-fsanitize=undefined','-fno-sanitize-recover=all','-I'+str(ROOT),str(p),'-o',str(OUT/f'root-{name}')]);result=run([str(OUT/f'root-{name}')]);(OUT/f'root-{name}.log').write_text(result.stdout+result.stderr);print(result.stdout.strip(),flush=True)

def main():
 OUT.mkdir(parents=True,exist_ok=True)
 previous=SOURCE.parent/'60-image-handles'
 for p in previous.rglob('*'):
  if p.is_file() and str(p.relative_to(previous))not in {'CMakeLists.txt','README.md','src/main.cpp','renderer/image_store.h'}:assert p.read_bytes()==(SOURCE/p.relative_to(previous)).read_bytes(),p
 env={**os.environ,'SDL_VIDEODRIVER':'dummy','SDL_AUDIODRIVER':'dummy','DISPLAY':'','WAYLAND_DISPLAY':''}
 for backend in ['SCRIPTED','SDL']:
  build=OUT/backend.lower();run(['cmake','-S',str(SOURCE),'-B',str(build),'-DSTUDY_PLATFORM='+backend,'-DCMAKE_BUILD_TYPE=Release','-DCMAKE_CXX_FLAGS=-Wall -Wextra -Wpedantic'])
  result=run(['cmake','--build',str(build),'-j3']);(OUT/f'build-{backend}.log').write_text(result.stdout+result.stderr);assert 'warning:'not in result.stdout+result.stderr
  result=run(['ctest','--test-dir',str(build),'--output-on-failure'],env=env);(OUT/f'ctest-{backend}.log').write_text(result.stdout);print(result.stdout.strip(),flush=True)
  print(run([str(build/'transform_demo')]).stdout.strip(),flush=True)
  if backend=='SDL':
   result=run([str(build/'transform_real')],env={**env,'SDL_VIDEODRIVER':'offscreen','LIBGL_ALWAYS_SOFTWARE':'1'});(OUT/'pixels.log').write_text(result.stdout+result.stderr);print(result.stdout.strip(),flush=True)
 flags=['c++','-std=c++17','-O1','-DNDEBUG','-fsanitize=undefined','-fno-sanitize-recover=all','-I'+str(SOURCE)]
 run([*flags,str(SOURCE/'tests/image_geometry_contract.cpp'),'-o',str(OUT/'geometry-sanitized')]);print(run([str(OUT/'geometry-sanitized')]).stdout.strip(),flush=True)
 run([*flags,str(SOURCE/'tests/image_quad_contract.cpp'),str(SOURCE/'renderer/program.cpp'),str(SOURCE/'renderer/shader.cpp'),'-o',str(OUT/'quad-sanitized')]);print(run([str(OUT/'quad-sanitized')]).stdout.strip(),flush=True)
 for name,old,new in [('direction','sine * offset_x + cosine * offset_y','-sine * offset_x + cosine * offset_y'),('flip','static_cast<double>(uv.u1)','static_cast<double>(uv.u0)')]:
  folder=OUT/name;header=folder/'renderer/image_geometry.h';header.parent.mkdir(parents=True,exist_ok=True);text=(SOURCE/'renderer/image_geometry.h').read_text();assert old in text;header.write_text(text.replace(old,new))
  run(['c++','-std=c++17','-O2','-DNDEBUG','-I'+str(folder),str(SOURCE/'tests/image_geometry_contract.cpp'),'-o',str(folder/'check')]);result=subprocess.run([str(folder/'check')],capture_output=True,text=True);assert result.returncode==1 and 'CHECK failed'in result.stderr;(folder/'result.log').write_text(result.stderr)
 root_probe()
 from check_learning_handles import root_probe as handle_probe
 handle_probe()
 build=ROOT/'out/learning-checkpoints/54-game-adapter-check/root';run(['cmake','-S',str(ROOT),'-B',str(build),'-DTETRIS_BUILD_REACTOR=OFF','-DCMAKE_BUILD_TYPE=Release']);run(['cmake','--build',str(build),'-j3'])
 result=run(['ctest','--test-dir',str(build),'--output-on-failure'],env=env);(OUT/'root-ctest.log').write_text(result.stdout);print(result.stdout.strip(),flush=True)
 assert run([str(build/'sim_hash_dump')]).stdout==(ROOT/'python/tests/_sim_hash_dump.txt').read_text()
 print('Image UV/tint/pivot geometry, GL state/failure paths, scalar pixel oracle and root rotations/regressions passed.',flush=True)
if __name__=='__main__':main()
