"""Validate candidate movement, same-name GPU updates, input and actual pixels."""
from pathlib import Path
import json, os, subprocess
ROOT=Path(__file__).resolve().parents[1]
SOURCE=ROOT/'docs/learn/checkpoints/29-horizontal-move'
OUT=ROOT/'out/learning-checkpoints/29-horizontal-move-check'
def run(args,**kw):
    r=subprocess.run(args,cwd=ROOT,text=True,capture_output=True,timeout=60,**kw)
    if r.returncode:raise RuntimeError(f'{args}\n{r.stdout}\n{r.stderr}')
    return r

def main():
    OUT.mkdir(parents=True,exist_ok=True)
    env={k:v for k,v in os.environ.items() if k not in ['LD_PRELOAD','LEARN_GL','LEARN_SMOKE']}
    cpuenv={**env,'SDL_VIDEODRIVER':'unavailable','DISPLAY':'','WAYLAND_DISPLAY':''}
    expected='''step=-1 column=3 -> 2 result=moved
step=-1 column=2 -> 1 result=moved
step=-1 column=1 -> 0 result=moved
step=-1 column=0 -> 0 result=blocked
step=0 column=0 -> 0 result=idle
step=1 column=0 -> 1 result=moved'''
    for backend in ['SCRIPTED','SDL']:
        b=OUT/backend.lower()
        run(['cmake','-S',str(SOURCE),'-B',str(b),'-DSTUDY_PLATFORM='+backend,'-DCMAKE_BUILD_TYPE=Release','-DCMAKE_EXPORT_COMPILE_COMMANDS=ON','-DCMAKE_CXX_FLAGS=-Wall -Wextra -Wpedantic'])
        r=run(['cmake','--build',str(b),'-j3']);assert 'warning:' not in r.stderr,r.stderr
        print(run(['ctest','--test-dir',str(b),'--output-on-failure']).stdout.strip(),flush=True)
        for name in ['movement_demo','movement_contract']:
            link=(b/'CMakeFiles'/f'{name}.dir/link.txt').read_text()
            assert all(x not in link for x in ['SDL','study_gl','study_platform']),link
        out=run([str(b/'movement_demo')],env=cpuenv).stdout.strip();assert out==expected,out
        print(out,flush=True)
        if backend=='SDL':
            assert 'Usage:' in run([str(b/'tetris'),'--help'],env=cpuenv).stdout
            print(run([str(b/'movement_probe'),str(OUT/'frame')],env={**env,'SDL_VIDEODRIVER':'offscreen'}).stdout.strip(),flush=True)
    flags=['-std=c++17','-DNDEBUG','-fsanitize=undefined','-fno-sanitize-recover=all']
    run(['c++',*flags,'-I'+str(SOURCE),str(SOURCE/'tests/movement_contract.cpp'),'-o',str(OUT/'sanitized')])
    print(run([str(OUT/'sanitized')]).stdout.strip(),flush=True)
    run(['c++',*flags,'-I'+str(ROOT),'-I'+str(SOURCE),str(ROOT/'tests/learning/current_movement.cpp'),str(ROOT/'src/sim_game.cpp'),str(ROOT/'src/position.cpp'),'-o',str(OUT/'current_movement')])
    print(run([str(OUT/'current_movement')]).stdout.strip(),flush=True)
    m=OUT/'mutation';(m/'simulation').mkdir(parents=True,exist_ok=True)
    s=(SOURCE/'simulation/movement.h').read_text();needle='const auto cells = study_piece::to_board(candidate);';assert needle in s
    (m/'simulation/movement.h').write_text(s.replace(needle,'current = candidate;\n    '+needle))
    run(['c++','-std=c++17','-DNDEBUG','-I'+str(m),'-I'+str(SOURCE),str(SOURCE/'tests/movement_contract.cpp'),'-o',str(OUT/'early_commit')])
    r=subprocess.run([str(OUT/'early_commit')],text=True,capture_output=True,timeout=5)
    assert r.returncode==1 and 'movement line' in r.stderr,r
    print('Early-commit mutation rejected in Release; GPU store and framebuffer updates verified.',flush=True)
if __name__=='__main__':main()
