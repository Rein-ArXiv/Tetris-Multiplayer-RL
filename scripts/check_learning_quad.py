"""Validate six-vertex extents, shared edges, winding and real GL culling."""
from pathlib import Path
import json,os,subprocess
ROOT=Path(__file__).resolve().parents[1]
SOURCE=ROOT/'docs/learn/checkpoints/21-quad'
OUT=ROOT/'out/learning-checkpoints/21-quad-check'
def run(args,**kwargs):
 r=subprocess.run(args,cwd=ROOT,text=True,capture_output=True,timeout=60,**kwargs)
 if r.returncode:raise RuntimeError(f'{args}\n{r.stdout}\n{r.stderr}')
 return r
def main():
 for backend in ['SCRIPTED','SDL']:
  build=OUT/backend.lower()
  run(['cmake','-S',str(SOURCE),'-B',str(build),'-DSTUDY_PLATFORM='+backend,'-DCMAKE_BUILD_TYPE=Release','-DCMAKE_EXPORT_COMPILE_COMMANDS=ON','-DCMAKE_CXX_FLAGS=-Wall -Wextra -Wpedantic'])
  r=run(['cmake','--build',str(build),'-j2']);assert 'warning:' not in r.stderr,r.stderr
  print(run(['ctest','--test-dir',str(build),'--output-on-failure']).stdout.strip())
  for row in json.loads((build/'compile_commands.json').read_text()):
   if '/renderer/' in row['file'] or 'quad_demo' in row['file']:assert 'SDL2' not in row['command'],row
  demo=run([str(build/'quad_demo')]).stdout;assert 'closed duplicates=8; strict holes=8; owned holes=0 duplicates=0' in demo;print(demo.strip())
  if backend=='SDL':
   env={k:v for k,v in os.environ.items() if k not in ['LD_PRELOAD','LEARN_GL','LEARN_SMOKE']}
   for args in [['missing'],['quad','extra']]:
    r=subprocess.run([str(build/'tetris'),*args],text=True,capture_output=True,timeout=5,env={**env,'SDL_VIDEODRIVER':'unavailable'});assert r.returncode==2 and 'Usage:' in r.stderr
   print(run([str(build/'quad_probe'),str(OUT/'quad')],env={**env,'SDL_VIDEODRIVER':'offscreen'}).stdout.strip())
 # ABI against actual Khronos declarations.
 abi=OUT/'abi.cpp';abi.write_text('#include <GL/glcorearb.h>\n#include <type_traits>\n#include "renderer/gl_api.h"\n'+''.join(f'static_assert(std::is_same<decltype(study_gl::GlApi::{n}), PFNGL{n.upper()}PROC>::value);\n' for n in ['Enable','Disable','FrontFace','CullFace'])+'int main(){}\n')
 run(['c++','-std=c++17','-I'+str(SOURCE),str(abi),'-o',str(OUT/'abi')])
 # Closed edges intentionally reintroduce double ownership; require rejection.
 mutation=OUT/'mutation';(mutation/'renderer').mkdir(parents=True,exist_ok=True)
 for n in ['mesh.h','raster.h']:(mutation/'renderer'/n).write_text((SOURCE/'renderer'/n).read_text())
 h=(SOURCE/'renderer/quad.h').read_text();assert 'return owned(a, b) && owned(b, c) && owned(c, a);' in h
 (mutation/'renderer/quad.h').write_text(h.replace('return owned(a, b) && owned(b, c) && owned(c, a);','return true;'))
 run(['c++','-std=c++17','-DNDEBUG','-I'+str(mutation),str(SOURCE/'tests/quad_contract.cpp'),'-o',str(OUT/'closed_edges')])
 r=subprocess.run([str(OUT/'closed_edges')],text=True,capture_output=True,timeout=5);assert r.returncode==1 and 'quad line' in r.stderr
 print('Four ABI signatures match; double-owner negative control rejected. Physical GPU/native presentation not claimed.')
if __name__=='__main__':main()
