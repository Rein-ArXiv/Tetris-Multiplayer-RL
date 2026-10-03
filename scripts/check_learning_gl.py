"""Check GL ownership/failures with explicit doubles, then probe real offscreen SDL.

Linux only. The double proves cleanup and API policy, never GPU rendering.
Offscreen may provide a single-buffer surface, which this windowed lesson rejects.
"""
import json
import os
from pathlib import Path
import shlex
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / 'docs/learn/checkpoints/11-gl-context'
OUT = ROOT / 'out/learning-checkpoints/11-gl-context'

def run(args, **kw):
    r = subprocess.run(args,cwd=ROOT,text=True,capture_output=True,timeout=60,**kw)
    if r.returncode: raise RuntimeError(f'{args}\n{r.stdout}\n{r.stderr}')
    return r

def main():
    if not sys.platform.startswith('linux'): raise SystemExit('Linux-only interposer test')
    OUT.mkdir(parents=True,exist_ok=True)
    flags = shlex.split(run(['pkg-config','--cflags','--libs','sdl2']).stdout)
    run(['cmake','-S',str(SOURCE),'-B',str(OUT),'-DSTUDY_PLATFORM=SDL',
         '-DCMAKE_CXX_FLAGS=-Wall -Wextra -Wpedantic','-DCMAKE_EXPORT_COMPILE_COMMANDS=ON'])
    build = run(['cmake','--build',str(OUT),'-j2'])
    assert 'warning:' not in build.stderr, build.stderr
    cmds = json.loads((OUT/'compile_commands.json').read_text())
    assert 'SDL2' not in next(c['command'] for c in cmds if c['file'].endswith('/src/main.cpp'))
    probe = OUT/'gl_probe.so'
    run(['cc','-shared','-fPIC','-Wall','-Wextra',str(ROOT/'tests/learning/gl_probe.c'),*flags,'-ldl','-o',str(probe)])
    base = {k:v for k,v in os.environ.items() if k not in ['LD_PRELOAD','LEARN_GL','SDL_VIDEODRIVER']}
    cases = {
        'init-fail': [],'attribute-fail': [],'window-fail': [],
        'context-fail':['window'],
        **{m:['context','window'] for m in ['current-fail','current-window-fail','query-fail',
             'version-fail','profile-fail','buffer-fail','frequency-fail','okay','higher']}}
    count = 0
    for production in [False,True]:
        binary = OUT/('production_gl_contract' if production else 'gl_contract')
        run(['c++','-std=c++17','-Wall','-Wextra','-Wpedantic',
             *(['-DLEARN_PRODUCTION'] if production else []),'-I'+str(SOURCE),
             str(ROOT/'tests/learning/gl_lifetime.cpp'),
             str(ROOT/'platform/sdl.cpp') if production else str(SOURCE/'platform/sdl.cpp'),
             *flags,'-o',str(binary)])
        active_cases = cases if not production else {m:cases[m] for m in
                       ['init-fail','attribute-fail','window-fail','context-fail','frequency-fail','okay']}
        if production: active_cases = {**active_cases,'makecurrent-fail':['context','window'],'swap-fail':['context','window']}
        for mode,cleanup in active_cases.items():
            success = mode in ['okay','higher','swap-fail']
            r = run([str(binary),*([] if success else ['failure'])],
                    env={**base,'SDL_VIDEODRIVER':'dummy','LD_PRELOAD':str(probe),'LEARN_GL':mode})
            log = [line for line in r.stderr.splitlines() if line.startswith('glprobe:')]
            assert log == ['glprobe:'+v for v in ['init',*cleanup,'quit']] * (1 if production else 2), (mode,log)
            count += 1
    # Real SDL context: no interposer and no fabricated attributes.
    real = subprocess.run([str(OUT/'gl_contract')],cwd=ROOT,text=True,capture_output=True,
                          env={**base,'SDL_VIDEODRIVER':'offscreen'},timeout=15)
    if real.returncode == 0:
        print('Real offscreen context: requested window attributes accepted, two lifetimes passed')
    elif 'unsupported SDL GL config:' in real.stderr:
        retry = run([str(OUT/'gl_contract'),'failure'],env={**base,'SDL_VIDEODRIVER':'offscreen'})
        print('Real offscreen context created; SDL configuration rejected by window policy: '+next(
            s for s in retry.stderr.splitlines() if s.startswith('unsupported SDL GL config:')))
    else:
        raise RuntimeError('Real offscreen GL unavailable; do not count as passed:\n'+real.stderr)
    # Existing event contracts still operate with GL ownership present.
    binary = OUT/'input_probe'
    run(['c++','-std=c++17','-DLEARN_TEXT','-I'+str(SOURCE),str(ROOT/'tests/learning/platform_input.cpp'),
         str(SOURCE/'platform/sdl.cpp'),*flags,'-o',str(binary)])
    run([str(binary)],env={**base,'SDL_VIDEODRIVER':'dummy','LD_PRELOAD':str(probe),'LEARN_GL':'okay'})
    print(f'{count} injected GL paths + existing input contracts passed. No native GUI/GPU presentation tested.')

if __name__ == '__main__': main()
