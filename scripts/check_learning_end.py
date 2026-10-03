"""Verify end-state boundaries, production policy contrast and real GL end screens."""
from pathlib import Path
import os, subprocess
ROOT=Path(__file__).resolve().parents[1]
SOURCE=ROOT/'docs/learn/checkpoints/37-game-over'
OUT=ROOT/'out/learning-checkpoints/37-game-over-check'
def run(args,**kw):
    r=subprocess.run(args,cwd=ROOT,text=True,capture_output=True,timeout=kw.pop('timeout',60),**kw)
    if r.returncode:raise RuntimeError(f'{args}\n{r.stdout}\n{r.stderr}')
    return r

def main():
    OUT.mkdir(parents=True,exist_ok=True)
    env={k:v for k,v in os.environ.items() if k not in ['LD_PRELOAD','LEARN_GL','LEARN_SMOKE']}
    cpu={**env,'SDL_VIDEODRIVER':'unavailable','DISPLAY':'','WAYLAND_DISPLAY':''}
    for backend in ['SCRIPTED','SDL']:
        b=OUT/backend.lower()
        run(['cmake','-S',str(SOURCE),'-B',str(b),'-DSTUDY_PLATFORM='+backend,'-DCMAKE_BUILD_TYPE=Release','-DCMAKE_CXX_FLAGS=-Wall -Wextra -Wpedantic'])
        r=run(['cmake','--build',str(b),'-j3'],timeout=180);assert 'warning:' not in r.stderr,r.stderr
        print(run(['ctest','--test-dir',str(b),'--output-on-failure']).stdout.strip(),flush=True)
        for name in ['end_demo','end_contract']:
            link=(b/'CMakeFiles'/f'{name}.dir/link.txt').read_text()
            assert all(x not in link for x in ['SDL','study_gl','study_platform']),link
        demo=run([str(b/'end_demo')],env=cpu).stdout.strip()
        expected='GAME OVER: initial spawn blocked. Escape to exit. | cleared=0 cursor=4 next=3\nGAME OVER: next spawn blocked after lock. Escape to exit. | cleared=0 cursor=5 next=3\nPLAYING | cleared=2 cursor=5 next=3'
        assert demo==expected,demo;print(demo,flush=True)
        if backend=='SDL':
            assert 'initial-blocked' in run([str(b/'tetris'),'--help'],env=cpu).stdout
            r=subprocess.run([str(b/'tetris'),'O','invalid'],env=cpu,text=True,capture_output=True,timeout=5)
            assert r.returncode==2 and 'Usage:' in r.stdout
            r=run([str(b/'end_probe'),str(OUT/'frame')],env={**env,'SDL_VIDEODRIVER':'offscreen'})
            (OUT/'gl.log').write_text(r.stdout)
            assert r.stdout.count('framebuffer matched')==126
            print('126 real GL frames: independent board/active/preview/X pixels; exact diagonal edges allow marker or background only',flush=True)
            print(run([str(b/'rotation_input')],env={**env,'SDL_VIDEODRIVER':'dummy'}).stdout.strip(),flush=True)
    flags=['-std=c++17','-O1','-DNDEBUG','-fsanitize=undefined','-fno-sanitize-recover=all']
    run(['c++',*flags,'-I'+str(SOURCE),str(SOURCE/'tests/end_contract.cpp'),'-o',str(OUT/'sanitized')])
    print(run([str(OUT/'sanitized')]).stdout.strip(),flush=True)
    run(['c++',*flags,'-I'+str(ROOT),'-I'+str(SOURCE),str(ROOT/'tests/learning/current_end.cpp'),str(ROOT/'src/sim_game.cpp'),str(ROOT/'src/position.cpp'),'-o',str(OUT/'current_end')])
    print(run([str(OUT/'current_end')]).stdout.strip(),flush=True)
    m=OUT/'mutation';(m/'simulation').mkdir(parents=True,exist_ok=True)
    s=(SOURCE/'simulation/round.h').read_text();needle='candidate.end_reason_=EndReason::spawn_blocked;';assert needle in s
    (m/'simulation/round.h').write_text(s.replace(needle,'/* missing end reason */'))
    run(['c++','-std=c++17','-O1','-DNDEBUG','-I'+str(m),'-I'+str(SOURCE),str(SOURCE/'tests/end_contract.cpp'),'-o',str(OUT/'missing_reason')])
    r=subprocess.run([str(OUT/'missing_reason')],text=True,capture_output=True,timeout=5)
    assert r.returncode==1 and 'end line' in r.stderr,r
    print('Missing-end-reason mutation rejected in Release.',flush=True)
if __name__=='__main__':main()
