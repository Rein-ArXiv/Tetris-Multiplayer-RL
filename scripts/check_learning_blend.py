"""Validate multi-draw passes, source-over arithmetic and actual RGBA readback."""
from pathlib import Path
import json,os,subprocess
ROOT=Path(__file__).resolve().parents[1]
SOURCE=ROOT/'docs/learn/checkpoints/22-blending'
OUT=ROOT/'out/learning-checkpoints/22-blending-check'
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
   if '/renderer/' in row['file'] or 'blend_demo' in row['file']:assert 'SDL2' not in row['command'],row
  demo=run([str(build/'blend_demo')]).stdout
  assert '0.25 0.00 0.50 0.75' in demo and '0.50 0.00 0.25 1.00' in demo
  print(demo.strip())
  if backend=='SDL':
   env={k:v for k,v in os.environ.items() if k not in ['LD_PRELOAD','LEARN_GL','LEARN_SMOKE']}
   for args in [['missing'],['ab','extra']]:
    r=subprocess.run([str(build/'tetris'),*args],text=True,capture_output=True,timeout=5,env={**env,'SDL_VIDEODRIVER':'unavailable'});assert r.returncode==2 and 'Usage:' in r.stderr
   print(run([str(build/'blend_probe'),str(OUT/'blend')],env={**env,'SDL_VIDEODRIVER':'offscreen'}).stdout.strip())
   # The old convenience path still clears/draws correctly after extraction.
   print(run([str(build/'quad_probe')],env={**env,'SDL_VIDEODRIVER':'offscreen'}).stdout.strip())
 abi=OUT/'abi.cpp';abi.write_text('#include <GL/glcorearb.h>\n#include <type_traits>\n#include "renderer/gl_api.h"\n'+''.join(f'static_assert(std::is_same<decltype(study_gl::GlApi::{n}), PFNGL{n.upper()}PROC>::value);\n' for n in ['BlendEquation','BlendFuncSeparate'])+'int main(){}\n')
 run(['c++','-std=c++17','-I'+str(SOURCE),str(abi),'-o',str(OUT/'abi')])
 mutation=OUT/'mutation';(mutation/'renderer').mkdir(parents=True,exist_ok=True)
 h=(SOURCE/'renderer/blend.h').read_text();assert 'out.a = src.a + dst.a * keep;' in h
 (mutation/'renderer/blend.h').write_text(h.replace('out.a = src.a + dst.a * keep;','out.a = src.a * src.a + dst.a * keep;'))
 run(['c++','-std=c++17','-DNDEBUG','-I'+str(mutation),str(SOURCE/'tests/blend_contract.cpp'),'-o',str(OUT/'wrong_alpha')])
 r=subprocess.run([str(OUT/'wrong_alpha')],text=True,capture_output=True,timeout=5);assert r.returncode==1 and 'blend line' in r.stderr
 print('Two ABI signatures match; squared-alpha negative control rejected. Physical GPU/native presentation not claimed.')
if __name__=='__main__':main()
