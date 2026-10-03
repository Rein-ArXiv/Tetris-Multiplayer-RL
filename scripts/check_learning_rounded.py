"""Image transform, UV/tint vertices, blend pixels and bounded rotation."""
from pathlib import Path
import os,subprocess
ROOT=Path(__file__).resolve().parents[1]
SOURCE=ROOT/'docs/learn/checkpoints/62-rounded-corners'
OUT=ROOT/'out/learning-checkpoints/62-rounded-corners-check'
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
 source=OUT/'before-renderer.cpp' if before else ROOT/'renderer/renderer.cpp'
 code=r"""#include "renderer/renderer.h"
#include <cmath>
#include <limits>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
using GLuint=unsigned;static GLuint s_white=1;static int calls=0;static float radius=0;
void glb_rect(GLuint,float,float,float,float,float,float,float,float,Color,float r,float){++calls;radius=r;}
#define CHECK(e) do{if(!(e)){std::fprintf(stderr,"CHECK failed %d\n",__LINE__);std::exit(1);}}while(false)
"""+cut(source.read_text(),'void draw_rect_rounded(')+r"""
int main(){draw_rect_rounded(0,0,40,20,std::numeric_limits<float>::quiet_NaN(),WHITE);
"""
 if before:code+=r"""CHECK(calls==1&&std::isnan(radius));std::puts("BEFORE: NaN roundness reaches vertex queue as NaN radius");}
"""
 else:code+=r"""CHECK(calls==0);
 for(float r:{std::numeric_limits<float>::infinity(),-std::numeric_limits<float>::infinity()})draw_rect_rounded(0,0,40,20,r,WHITE);
 draw_rect_rounded(0,0,0,20,1,WHITE);draw_rect_rounded(0,0,40,-1,1,WHITE);CHECK(calls==0);
 draw_rect_rounded(0,0,40,20,.5f,WHITE);CHECK(calls==1&&radius==5);
 draw_rect_rounded(0,0,40,20,2,WHITE);CHECK(calls==2&&radius==10);
 draw_rect_rounded(0,0,40,20,-1,WHITE);CHECK(calls==3&&radius==0);
 draw_rect_rounded(0,0,40,20,.05f,WHITE);CHECK(calls==4&&radius==0);
 draw_rect_rounded(0,0,40,20,.1f,WHITE);CHECK(calls==5&&radius==1);
 std::puts("AFTER: nonfinite/invalid extent rejected, finite clamp and subunit approximation retained");}
"""
 name='before'if before else'after';p=OUT/f'root-{name}.cpp';p.write_text(code)
 run(['c++','-std=c++17','-O1','-DNDEBUG','-fsanitize=undefined','-fno-sanitize-recover=all','-I'+str(ROOT),str(p),'-o',str(OUT/f'root-{name}')]);r=run([str(OUT/f'root-{name}')]);(OUT/f'root-{name}.log').write_text(r.stdout+r.stderr);print(r.stdout.strip(),flush=True)

def main():
 OUT.mkdir(parents=True,exist_ok=True)
 previous=SOURCE.parent/'61-image-transform'
 for p in previous.rglob('*'):
  if p.is_file() and str(p.relative_to(previous))not in {'CMakeLists.txt','README.md','src/main.cpp','renderer/image_store.h'}:assert p.read_bytes()==(SOURCE/p.relative_to(previous)).read_bytes(),p
 env={**os.environ,'SDL_VIDEODRIVER':'dummy','SDL_AUDIODRIVER':'dummy','DISPLAY':'','WAYLAND_DISPLAY':''}
 for backend in ['SCRIPTED','SDL']:
  build=OUT/backend.lower();run(['cmake','-S',str(SOURCE),'-B',str(build),'-DSTUDY_PLATFORM='+backend,'-DCMAKE_BUILD_TYPE=Release','-DCMAKE_CXX_FLAGS=-Wall -Wextra -Wpedantic'])
  result=run(['cmake','--build',str(build),'-j3']);(OUT/f'build-{backend}.log').write_text(result.stdout+result.stderr);assert 'warning:'not in result.stdout+result.stderr
  result=run(['ctest','--test-dir',str(build),'--output-on-failure'],env=env);(OUT/f'ctest-{backend}.log').write_text(result.stdout);print(result.stdout.strip(),flush=True)
  print(run([str(build/'rounded_demo')]).stdout.strip(),flush=True)
  if backend=='SDL':
   result=run([str(build/'rounded_real')],env={**env,'SDL_VIDEODRIVER':'offscreen','LIBGL_ALWAYS_SOFTWARE':'1'});(OUT/'pixels.log').write_text(result.stdout+result.stderr);print(result.stdout.strip(),flush=True)
 flags=['c++','-std=c++17','-O1','-DNDEBUG','-fsanitize=undefined','-fno-sanitize-recover=all','-I'+str(SOURCE)]
 run([*flags,str(SOURCE/'tests/rounded_contract.cpp'),'-o',str(OUT/'geometry-sanitized')]);print(run([str(OUT/'geometry-sanitized')]).stdout.strip(),flush=True)
 run([*flags,str(SOURCE/'tests/rounded_quad_contract.cpp'),str(SOURCE/'renderer/program.cpp'),str(SOURCE/'renderer/shader.cpp'),'-o',str(OUT/'quad-sanitized')]);print(run([str(OUT/'quad-sanitized')]).stdout.strip(),flush=True)
 for name,old,new in [('sign','return outside + inside - radius;','return outside + inside + radius;'),('opacity','return 1.0 - t * t * (3.0 - 2.0 * t);','return t * t * (3.0 - 2.0 * t);')]:
  folder=OUT/name;header=folder/'renderer/rounded_distance.h';header.parent.mkdir(parents=True,exist_ok=True);text=(SOURCE/'renderer/rounded_distance.h').read_text();assert old in text;header.write_text(text.replace(old,new))
  run(['c++','-std=c++17','-O2','-DNDEBUG','-I'+str(folder),'-I'+str(SOURCE),str(SOURCE/'tests/rounded_contract.cpp'),'-o',str(folder/'check')]);result=subprocess.run([str(folder/'check')],capture_output=True,text=True);assert result.returncode==1 and 'CHECK failed'in result.stderr;(folder/'result.log').write_text(result.stderr)
 root_probe()
 from check_learning_transform import root_probe as transform_probe
 transform_probe()
 from check_learning_handles import root_probe as handle_probe
 handle_probe()
 build=ROOT/'out/learning-checkpoints/54-game-adapter-check/root';run(['cmake','-S',str(ROOT),'-B',str(build),'-DTETRIS_BUILD_REACTOR=OFF','-DCMAKE_BUILD_TYPE=Release']);run(['cmake','--build',str(build),'-j3'])
 result=run(['ctest','--test-dir',str(build),'--output-on-failure'],env=env);(OUT/'root-ctest.log').write_text(result.stdout);print(result.stdout.strip(),flush=True)
 assert run([str(build/'sim_hash_dump')]).stdout==(ROOT/'python/tests/_sim_hash_dump.txt').read_text()
 print('Rounded distance/mask, vertex contract, GL failures, independent pixels and root regressions passed.',flush=True)
if __name__=='__main__':main()
