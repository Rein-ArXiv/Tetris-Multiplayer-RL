"""Validate placement occupancy, failure preservation, input and actual pixels."""
from pathlib import Path
import json, os, subprocess
ROOT=Path(__file__).resolve().parents[1]
SOURCE=ROOT/'docs/learn/checkpoints/30-collision'
OUT=ROOT/'out/learning-checkpoints/30-collision-check'
def run(args,**kw):
    r=subprocess.run(args,cwd=ROOT,text=True,capture_output=True,timeout=60,**kw)
    if r.returncode:raise RuntimeError(f'{args}\n{r.stdout}\n{r.stderr}')
    return r

def main():
    OUT.mkdir(parents=True,exist_ok=True)
    env={k:v for k,v in os.environ.items() if k not in ['LD_PRELOAD','LEARN_GL','LEARN_SMOKE']}
    cpuenv={**env,'SDL_VIDEODRIVER':'unavailable','DISPLAY':'','WAYLAND_DISPLAY':''}
    expected='''placement column=3: clear
placement column=2: occupied
placement column=-1: outside
placement column=8: outside
step=-1 column=3 -> 3 result=blocked
step=0 column=3 -> 3 result=idle
step=1 column=3 -> 4 result=moved
step=-1 column=4 -> 3 result=moved
step=-1 column=3 -> 3 result=blocked'''
    for backend in ['SCRIPTED','SDL']:
        b=OUT/backend.lower()
        run(['cmake','-S',str(SOURCE),'-B',str(b),'-DSTUDY_PLATFORM='+backend,'-DCMAKE_BUILD_TYPE=Release','-DCMAKE_EXPORT_COMPILE_COMMANDS=ON','-DCMAKE_CXX_FLAGS=-Wall -Wextra -Wpedantic'])
        r=run(['cmake','--build',str(b),'-j3']);assert 'warning:' not in r.stderr,r.stderr
        print(run(['ctest','--test-dir',str(b),'--output-on-failure']).stdout.strip(),flush=True)
        for name in ['collision_demo','collision_contract']:
            link=(b/'CMakeFiles'/f'{name}.dir/link.txt').read_text()
            assert all(x not in link for x in ['SDL','study_gl','study_platform']),link
        out=run([str(b/'collision_demo')],env=cpuenv).stdout.strip();assert out==expected,out
        print(out,flush=True)
        if backend=='SDL':
            assert 'Usage:' in run([str(b/'tetris'),'--help'],env=cpuenv).stdout
            print(run([str(b/'collision_probe'),str(OUT/'frame')],env={**env,'SDL_VIDEODRIVER':'offscreen'}).stdout.strip(),flush=True)
    flags=['-std=c++17','-DNDEBUG','-fsanitize=undefined','-fno-sanitize-recover=all']
    run(['c++',*flags,'-I'+str(SOURCE),str(SOURCE/'tests/collision_contract.cpp'),'-o',str(OUT/'sanitized')])
    print(run([str(OUT/'sanitized')]).stdout.strip(),flush=True)
    run(['c++',*flags,'-I'+str(ROOT),'-I'+str(SOURCE),str(ROOT/'tests/learning/current_collision.cpp'),str(ROOT/'src/position.cpp'),'-o',str(OUT/'current_collision')])
    print(run([str(OUT/'current_collision')]).stdout.strip(),flush=True)
    m=OUT/'mutation';(m/'simulation').mkdir(parents=True,exist_ok=True)
    s=(SOURCE/'simulation/collision.h').read_text();needle='if (!board.is_empty(cell.row, cell.column))';assert needle in s
    (m/'simulation/collision.h').write_text(s.replace(needle,'if (false)'))
    run(['c++','-std=c++17','-DNDEBUG','-I'+str(m),'-I'+str(SOURCE),str(SOURCE/'tests/collision_contract.cpp'),'-o',str(OUT/'ignore_occupied')])
    r=subprocess.run([str(OUT/'ignore_occupied')],text=True,capture_output=True,timeout=5)
    assert r.returncode==1 and 'collision line' in r.stderr,r
    print('Ignored-occupancy mutation rejected in Release; source helper parity and actual obstacle blocking verified.',flush=True)
if __name__=='__main__':main()
