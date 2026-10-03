"""Check all-or-nothing locking, spawn and terminal state and framebuffer output."""
from pathlib import Path
import os, subprocess
ROOT=Path(__file__).resolve().parents[1]
SOURCE=ROOT/'docs/learn/checkpoints/32-locking'
OUT=ROOT/'out/learning-checkpoints/32-locking-check'
def run(args,**kw):
    r=subprocess.run(args,cwd=ROOT,text=True,capture_output=True,timeout=60,**kw)
    if r.returncode:raise RuntimeError(f'{args}\n{r.stdout}\n{r.stderr}')
    return r

def main():
    OUT.mkdir(parents=True,exist_ok=True)
    env={k:v for k,v in os.environ.items() if k not in ['LD_PRELOAD','LEARN_GL','LEARN_SMOKE']}
    cpuenv={**env,'SDL_VIDEODRIVER':'unavailable','DISPLAY':'','WAYLAND_DISPLAY':''}
    for backend in ['SCRIPTED','SDL']:
        b=OUT/backend.lower()
        run(['cmake','-S',str(SOURCE),'-B',str(b),'-DSTUDY_PLATFORM='+backend,'-DCMAKE_BUILD_TYPE=Release','-DCMAKE_CXX_FLAGS=-Wall -Wextra -Wpedantic'])
        r=run(['cmake','--build',str(b),'-j3']);assert 'warning:' not in r.stderr,r.stderr
        print(run(['ctest','--test-dir',str(b),'--output-on-failure']).stdout.strip(),flush=True)
        for name in ['locking_demo','locking_contract']:
            link=(b/'CMakeFiles'/f'{name}.dir/link.txt').read_text()
            assert all(x not in link for x in ['SDL','study_gl','study_platform']),link
        out=run([str(b/'locking_demo')],env=cpuenv).stdout.strip()
        expected='\n'.join(f'tick={t} locks={i+1} filled={8+i*4} active='+('spawned' if i<4 else 'none (game over)') for i,t in enumerate([270,480,630,720,750]))
        assert out==expected,out;print(out,flush=True)
        if backend=='SDL':
            assert 'Usage:' in run([str(b/'tetris'),'--help'],env=cpuenv).stdout
            print(run([str(b/'locking_probe'),str(OUT/'frame')],env={**env,'SDL_VIDEODRIVER':'offscreen'}).stdout.strip(),flush=True)
    flags=['-std=c++17','-DNDEBUG','-fsanitize=undefined','-fno-sanitize-recover=all']
    run(['c++',*flags,'-I'+str(SOURCE),str(SOURCE/'tests/locking_contract.cpp'),'-o',str(OUT/'sanitized')])
    print(run([str(OUT/'sanitized')]).stdout.strip(),flush=True)
    run(['c++',*flags,'-I'+str(ROOT),'-I'+str(SOURCE),str(ROOT/'tests/learning/current_locking.cpp'),str(ROOT/'src/sim_game.cpp'),str(ROOT/'src/position.cpp'),'-o',str(OUT/'current_locking')])
    print(run([str(OUT/'current_locking')]).stdout.strip(),flush=True)
    m=OUT/'mutation';(m/'simulation').mkdir(parents=True,exist_ok=True)
    s=(SOURCE/'simulation/locking.h').read_text();needle='if ((*cells)[i].row == (*cells)[j].row &&';assert needle in s
    (m/'simulation/locking.h').write_text(s.replace(needle,'if (false && (*cells)[i].row == (*cells)[j].row &&'))
    run(['c++','-std=c++17','-DNDEBUG','-I'+str(m),'-I'+str(SOURCE),str(SOURCE/'tests/locking_contract.cpp'),'-o',str(OUT/'ignore_duplicate')])
    r=subprocess.run([str(OUT/'ignore_duplicate')],text=True,capture_output=True,timeout=5)
    assert r.returncode==1 and 'locking line' in r.stderr,r
    print('Duplicate-cell mutation rejected in Release; actual first-lock board parity and lock/spawn/game-over pixels passed.',flush=True)
if __name__=='__main__':main()
