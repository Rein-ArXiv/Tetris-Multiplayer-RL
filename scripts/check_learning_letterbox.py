"""CPU layout, SDL coordinate routing, actual GL pixel regions and mouse regression."""
from pathlib import Path
import json,os,subprocess,shlex
ROOT=Path(__file__).resolve().parents[1]
SOURCE=ROOT/'docs/learn/checkpoints/24-letterbox'
OUT=ROOT/'out/learning-checkpoints/24-letterbox-check'
def run(args,**kwargs):
    r=subprocess.run(args,cwd=ROOT,text=True,capture_output=True,timeout=60,**kwargs)
    if r.returncode:raise RuntimeError(f'{args}\n{r.stdout}\n{r.stderr}')
    return r

def main():
    env={k:v for k,v in os.environ.items() if k not in ['LD_PRELOAD','LEARN_GL','LEARN_SMOKE']}
    for backend in ['SCRIPTED','SDL']:
        build=OUT/backend.lower()
        run(['cmake','-S',str(SOURCE),'-B',str(build),'-DSTUDY_PLATFORM='+backend,'-DCMAKE_BUILD_TYPE=Release','-DCMAKE_EXPORT_COMPILE_COMMANDS=ON','-DCMAKE_CXX_FLAGS=-Wall -Wextra -Wpedantic'])
        r=run(['cmake','--build',str(build),'-j3']);assert 'warning:' not in r.stderr,r.stderr
        print(run(['ctest','--test-dir',str(build),'--output-on-failure']).stdout.strip())
        for row in json.loads((build/'compile_commands.json').read_text()):
            if '/renderer/' in row['file'] or 'letterbox_demo' in row['file']:assert 'SDL2' not in row['command'],row
        print(run([str(build/'letterbox_demo')]).stdout.strip())
        if backend=='SDL':
            print(run([str(build/'letterbox_probe'),str(OUT/'frame')],env={**env,'SDL_VIDEODRIVER':'offscreen'}).stdout.strip())
    flags=shlex.split(run(['pkg-config','--cflags','--libs','sdl2']).stdout)
    fixture=OUT/'coordinate_fixture.so';binary=OUT/'platform_letterbox'
    run(['cc','-shared','-fPIC',str(ROOT/'tests/learning/letterbox_probe.c'),*flags,'-ldl','-o',str(fixture)])
    run(['c++','-std=c++17','-Wall','-Wextra','-Wpedantic','-I'+str(SOURCE),str(SOURCE/'tests/platform_letterbox.cpp'),str(SOURCE/'platform/sdl.cpp'),*flags,'-ldl','-o',str(binary)])
    print(run([str(binary)],env={**env,'SDL_VIDEODRIVER':'dummy','LD_PRELOAD':str(fixture),'LEARN_GL':'okay'}).stdout.strip())
    # Regression helper shared by the actual SDL and Win32 adapters.
    run(['c++','-std=c++17','-DNDEBUG','-fsanitize=undefined','-fno-sanitize-recover=all','-I'+str(ROOT),str(ROOT/'tests/learning/mouse_coordinates.cpp'),'-o',str(OUT/'mouse_coordinates')])
    print(run([str(OUT/'mouse_coordinates')]).stdout.strip())
    run(['c++','-std=c++17','-Wall','-Wextra','-Wpedantic',str(ROOT/'tests/learning/current_platform_focus.cpp'),*flags,'-o',str(OUT/'production_sdl')])
    print(run([str(OUT/'production_sdl')]).stdout.strip())
    abi=OUT/'abi.cpp';abi.write_text('#include <GL/glcorearb.h>\n#include <type_traits>\n#include "renderer/gl_api.h"\nstatic_assert(std::is_same<decltype(study_gl::GlApi::Scissor),PFNGLSCISSORPROC>::value);\nint main(){}\n')
    run(['c++','-std=c++17','-I'+str(SOURCE),str(abi),'-o',str(OUT/'abi')])
    mutation=OUT/'mutation';(mutation/'renderer').mkdir(parents=True,exist_ok=True)
    s=(SOURCE/'renderer/letterbox.h').read_text();needle='l.drawable.height - l.viewport.y - l.viewport.height';assert needle in s
    (mutation/'renderer/letterbox.h').write_text(s.replace(needle,'l.viewport.y'))
    run(['c++','-std=c++17','-DNDEBUG','-I'+str(mutation),str(SOURCE/'tests/letterbox_contract.cpp'),'-o',str(OUT/'wrong_y')])
    r=subprocess.run([str(OUT/'wrong_y')],text=True,capture_output=True,timeout=5);assert r.returncode==1 and 'letterbox line' in r.stderr
    (mutation/'platform').mkdir(exist_ok=True)
    s=(ROOT/'platform/mouse_coordinates.h').read_text();needle='if (numerator < 0 && numerator % extent != 0) --result;';assert needle in s
    (mutation/'platform/mouse_coordinates.h').write_text(s.replace(needle,''))
    run(['c++','-std=c++17','-DNDEBUG','-I'+str(mutation),str(ROOT/'tests/learning/mouse_coordinates.cpp'),'-o',str(OUT/'truncated_mouse')])
    r=subprocess.run([str(OUT/'truncated_mouse')],text=True,capture_output=True,timeout=5);assert r.returncode==1 and 'mouse line' in r.stderr
    print('Scissor ABI, missing-y-flip and truncated-negative-input controls passed. Native resize/DPI/Win32 are separate.')
if __name__=='__main__':main()
