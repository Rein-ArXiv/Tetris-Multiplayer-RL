"""Check integer rotation, candidate occupancy and framebuffer output."""
from pathlib import Path
import os, subprocess
ROOT=Path(__file__).resolve().parents[1]
SOURCE=ROOT/'docs/learn/checkpoints/34-rotation'
OUT=ROOT/'out/learning-checkpoints/34-rotation-check'
def run(args,**kw):
    r=subprocess.run(args,cwd=ROOT,text=True,capture_output=True,timeout=kw.pop("timeout",60),**kw)
    if r.returncode:raise RuntimeError(f'{args}\n{r.stdout}\n{r.stderr}')
    return r

def main():
    OUT.mkdir(parents=True,exist_ok=True)
    env={k:v for k,v in os.environ.items() if k not in ['LD_PRELOAD','LEARN_GL','LEARN_SMOKE']}
    cpuenv={**env,'SDL_VIDEODRIVER':'unavailable','DISPLAY':'','WAYLAND_DISPLAY':''}
    for backend in ['SCRIPTED','SDL']:
        b=OUT/backend.lower()
        run(['cmake','-S',str(SOURCE),'-B',str(b),'-DSTUDY_PLATFORM='+backend,'-DCMAKE_BUILD_TYPE=Release','-DCMAKE_CXX_FLAGS=-Wall -Wextra -Wpedantic'])
        r=run(['cmake','--build',str(b),'-j3'],timeout=180);assert 'warning:' not in r.stderr,r.stderr
        print(run(['ctest','--test-dir',str(b),'--output-on-failure']).stdout.strip(),flush=True)
        for name in ['rotation_demo','rotation_contract']:
            link=(b/'CMakeFiles'/f'{name}.dir/link.txt').read_text()
            assert all(x not in link for x in ['SDL','study_gl','study_platform']),link
        out=run([str(b/'rotation_demo')],env=cpuenv).stdout.strip()
        expected='quarter=0 cells=(0,1)(1,0)(1,1)(1,2)\nquarter=1 cells=(0,1)(1,1)(1,2)(2,1)\nquarter=2 cells=(1,0)(1,1)(1,2)(2,1)\nquarter=3 cells=(0,1)(1,0)(1,1)(2,1)\nquarter=0 cells=(0,1)(1,0)(1,1)(1,2)\nfloor: blocked quarter=0 row=18'
        assert out==expected,out;print(out,flush=True)
        if backend=='SDL':
            assert 'Usage:' in run([str(b/'tetris'),'--help'],env=cpuenv).stdout
            print(run([str(b/'rotation_probe'),str(OUT/'frame')],env={**env,'SDL_VIDEODRIVER':'offscreen'}).stdout.strip(),flush=True)
            print(run([str(b/'rotation_input')],env={**env,'SDL_VIDEODRIVER':'dummy'}).stdout.strip(),flush=True)
    flags=['-std=c++17','-O1','-DNDEBUG','-fsanitize=undefined','-fno-sanitize-recover=all']
    run(['c++',*flags,'-I'+str(SOURCE),str(SOURCE/'tests/rotation_contract.cpp'),'-o',str(OUT/'sanitized')])
    print(run([str(OUT/'sanitized')]).stdout.strip(),flush=True)
    run(['c++',*flags,'-I'+str(ROOT),'-I'+str(SOURCE),str(ROOT/'tests/learning/current_rotation.cpp'),str(ROOT/'src/sim_game.cpp'),str(ROOT/'src/position.cpp'),'-o',str(OUT/'current_rotation')])
    print(run([str(OUT/'current_rotation')]).stdout.strip(),flush=True)
    m=OUT/'mutation';(m/'simulation').mkdir(parents=True,exist_ok=True)
    s=(SOURCE/'simulation/rotation_math.h').read_text();needle='pc2 - 2 * r + pr2';assert needle in s
    (m/'simulation/rotation_math.h').write_text(s.replace(needle,'pc2 + 2 * r + pr2'))
    run(['c++','-std=c++17','-O1','-DNDEBUG','-I'+str(m),'-I'+str(SOURCE),str(SOURCE/'tests/rotation_contract.cpp'),'-o',str(OUT/'wrong_sign')])
    r=subprocess.run([str(OUT/'wrong_sign')],text=True,capture_output=True,timeout=5)
    assert r.returncode==1 and 'rotation line' in r.stderr,r
    print('Wrong-sign mutation rejected in Release; production rotation parity and rotated pixels passed.',flush=True)
if __name__=='__main__':main()
