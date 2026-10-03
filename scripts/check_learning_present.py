"""Validate CPU stage clocks, GL boundaries, platform routing and real readback."""
from pathlib import Path
import json,os,subprocess,shlex
ROOT=Path(__file__).resolve().parents[1]
SOURCE=ROOT/'docs/learn/checkpoints/23-present'
OUT=ROOT/'out/learning-checkpoints/23-present-check'
def run(args,**kwargs):
 r=subprocess.run(args,cwd=ROOT,text=True,capture_output=True,timeout=60,**kwargs)
 if r.returncode:raise RuntimeError(f'{args}\n{r.stdout}\n{r.stderr}')
 return r
def main():
 env={k:v for k,v in os.environ.items() if k not in ['LD_PRELOAD','LEARN_GL','LEARN_SMOKE']}
 for backend in ['SCRIPTED','SDL']:
  build=OUT/backend.lower()
  run(['cmake','-S',str(SOURCE),'-B',str(build),'-DSTUDY_PLATFORM='+backend,'-DCMAKE_BUILD_TYPE=Release','-DCMAKE_EXPORT_COMPILE_COMMANDS=ON','-DCMAKE_CXX_FLAGS=-Wall -Wextra -Wpedantic'])
  r=run(['cmake','--build',str(build),'-j2']);assert 'warning:' not in r.stderr,r.stderr
  print(run(['ctest','--test-dir',str(build),'--output-on-failure']).stdout.strip())
  for row in json.loads((build/'compile_commands.json').read_text()):
   if '/renderer/' in row['file'] or 'timing_demo' in row['file']:assert 'SDL2' not in row['command'],row
  print(run([str(build/'timing_demo')]).stdout.strip())
  if backend=='SDL':
   for args in [['missing'],['submit','-1'],['flush','2'],['finish','0','extra']]:
    r=subprocess.run([str(build/'tetris'),*args],text=True,capture_output=True,timeout=5,env={**env,'SDL_VIDEODRIVER':'unavailable'});assert r.returncode==2 and 'Usage:' in r.stderr
   print(run([str(build/'present_probe')],env={**env,'SDL_VIDEODRIVER':'offscreen'}).stdout.strip())
   actual=subprocess.run([str(build/'tetris'),'submit','0'],text=True,capture_output=True,timeout=20,env={**env,'SDL_VIDEODRIVER':'offscreen'})
   if actual.returncode==0:print('Window demo: 120 presentation requests returned; display not measured')
   else:
    assert actual.returncode==1 and 'unsupported SDL GL config:' in actual.stderr,actual.stderr
    print('Window policy correctly rejects offscreen attributes: '+actual.stderr.strip())
 # The SDL adapter is tested with a labeled GL double, not a simulated display.
 flags=shlex.split(run(['pkg-config','--cflags','--libs','sdl2']).stdout)
 fixture=OUT/'present_fixture.so';binary=OUT/'platform_present'
 run(['cc','-shared','-fPIC',str(ROOT/'tests/learning/present_probe.c'),*flags,'-ldl','-o',str(fixture)])
 run(['c++','-std=c++17','-Wall','-Wextra','-Wpedantic','-I'+str(SOURCE),str(SOURCE/'tests/platform_present.cpp'),str(SOURCE/'platform/sdl.cpp'),*flags,'-ldl','-o',str(binary)])
 print(run([str(binary)],env={**env,'SDL_VIDEODRIVER':'dummy','LD_PRELOAD':str(fixture),'LEARN_GL':'okay'}).stdout.strip())
 abi=OUT/'abi.cpp';abi.write_text('#include <GL/glcorearb.h>\n#include <type_traits>\n#include "renderer/gl_api.h"\n'+''.join(f'static_assert(std::is_same<decltype(study_gl::GlApi::{n}), PFNGL{n.upper()}PROC>::value);\n' for n in ['Flush','Finish'])+'int main(){}\n')
 run(['c++','-std=c++17','-I'+str(SOURCE),str(abi),'-o',str(OUT/'abi')])
 mutation=OUT/'mutation';(mutation/'renderer').mkdir(parents=True,exist_ok=True)
 for name in ['submission.h','gl_api.h']:(mutation/'renderer'/name).write_text((SOURCE/'renderer'/name).read_text())
 h=(SOURCE/'renderer/cpu_timing.h').read_text();needle='ticks_to_ms(after_submit - begin, frequency)';assert needle in h
 (mutation/'renderer/cpu_timing.h').write_text(h.replace(needle,'(static_cast<double>(after_submit) - static_cast<double>(begin)) * 1000.0 / frequency'))
 run(['c++','-std=c++17','-DNDEBUG','-I'+str(mutation),str(SOURCE/'tests/present_contract.cpp'),'-o',str(OUT/'rounded_clock')])
 r=subprocess.run([str(OUT/'rounded_clock')],text=True,capture_output=True,timeout=5);assert r.returncode==1 and 'present line' in r.stderr
 print('Flush/Finish ABI and rounded-clock negative control passed. No physical GPU/display timing claimed.')
if __name__=='__main__':main()
