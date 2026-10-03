"""Validate teaching/production loaders separately from native window presentation."""
import json
import os
from pathlib import Path
import shlex
import subprocess
import sys
ROOT=Path(__file__).resolve().parents[1]
SOURCE=ROOT/'docs/learn/checkpoints/12-gl-loader'
OUT=ROOT/'out/learning-checkpoints/12-gl-loader'

def run(args,**kw):
    r=subprocess.run(args,cwd=ROOT,text=True,capture_output=True,timeout=60,**kw)
    if r.returncode: raise RuntimeError(f'{args}\n{r.stdout}\n{r.stderr}')
    return r

def main():
    OUT.mkdir(parents=True,exist_ok=True)
    flags=shlex.split(run(['pkg-config','--cflags','--libs','sdl2']).stdout)
    run(['cmake','-S',str(SOURCE),'-B',str(OUT),'-DSTUDY_PLATFORM=SDL',
         '-DCMAKE_CXX_FLAGS=-Wall -Wextra -Wpedantic','-DCMAKE_EXPORT_COMPILE_COMMANDS=ON'])
    r=run(['cmake','--build',str(OUT),'-j2']);assert 'warning:' not in r.stderr,r.stderr
    run(['ctest','--test-dir',str(OUT),'--output-on-failure'])
    print(run([str(OUT/'loader_contract')]).stdout.strip())
    for name,sources in [
        ('production_loader',['tests/learning/current_gl_loader.cpp','renderer/gl_api.cpp']),
        ('gl_abi',['tests/learning/gl_abi.cpp'])]:
        run(['c++','-std=c++17','-Wall','-Wextra','-Wpedantic',*sources,'-o',str(OUT/name)])
        r=run([str(OUT/name)]);print(r.stdout.strip() or 'Production signatures match installed Khronos declarations (Linux types only)')
    # Compile actual WGL source fragments against controlled return-value stubs.
    # This checks control flow only; it does not substitute a Windows toolchain.
    win=(ROOT/'platform/win32.cpp').read_text()
    lookup=win[win.index('void* platform_gl_get_proc('):win.index('void platform_viewport(')]
    start=win.index('    HGLRC legacy =')
    bootstrap=win[start:win.index('    ShowWindow(',start)]
    template=(ROOT/'tests/learning/wgl_lookup_template.cpp').read_text()
    (OUT/'wgl_lookup.cpp').write_text(template.replace('/* LOOKUP_BODY */',lookup).replace('/* BOOTSTRAP_BODY */',bootstrap))
    run(['c++','-std=c++17','-Wall','-Wextra','-Wpedantic',str(OUT/'wgl_lookup.cpp'),'-o',str(OUT/'wgl_lookup')])
    for mode in range(15): run([str(OUT/'wgl_lookup'),str(mode)])
    print('WGL source-fragment model: 15 return-value/sentinel paths passed (not native Windows)')
    script=OUT.parent/'12-gl-loader-scripted'
    run(['cmake','-S',str(SOURCE),'-B',str(script),'-DSTUDY_PLATFORM=SCRIPTED'])
    run(['cmake','--build',str(script),'-j2']);run(['ctest','--test-dir',str(script),'--output-on-failure'])
    output=run([str(script/'input_demo')]).stdout.splitlines()
    assert output == ['left pressed','text byte=65','left released','text byte=66','text byte=67',
                      'text dropped=0','left held=0 right held=0','elapsed=1.000 frames=4 events=4 last_dt=0.250000'],output
    assert not (script/'tetris').exists(),'Input-only backend must not advertise GL demo'
    commands=json.loads((OUT/'compile_commands.json').read_text())
    loader_cmd=next(r['command'] for r in commands if r['file'].endswith('/renderer/gl_api.cpp'))
    assert 'SDL2' not in loader_cmd,'Loader must not depend on SDL headers'
    binary=OUT/'loader_real'
    run(['c++','-std=c++17','-Wall','-Wextra','-Wpedantic','-I'+str(SOURCE),
         'tests/learning/loader_real.cpp',str(SOURCE/'renderer/gl_api.cpp'),*flags,'-o',str(binary)])
    env={k:v for k,v in os.environ.items() if k not in ['LD_PRELOAD','LEARN_GL','LEARN_SMOKE']}
    print(run([str(binary)],env={**env,'SDL_VIDEODRIVER':'offscreen'}).stdout.strip())
    print('SDL/SCRIPTED build, lookup contracts, production loader and actual offscreen queries passed; native GUI/Windows/macOS not tested.')
if __name__=='__main__':main()
