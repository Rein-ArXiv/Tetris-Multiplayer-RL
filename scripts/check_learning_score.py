"""Verify score transitions, bounded counters, decimal geometry and real GL pixels."""
from pathlib import Path
import os, subprocess
ROOT=Path(__file__).resolve().parents[1]
SOURCE=ROOT/'docs/learn/checkpoints/38-score'
OUT=ROOT/'out/learning-checkpoints/38-score-check'
def run(args,**kw):
    r=subprocess.run(args,cwd=ROOT,text=True,capture_output=True,timeout=kw.pop('timeout',60),**kw)
    if r.returncode:raise RuntimeError(f'{args}\n{r.stdout}\n{r.stderr}')
    return r

def main():
    OUT.mkdir(parents=True,exist_ok=True)
    env={k:v for k,v in os.environ.items() if k not in ['LD_PRELOAD','LEARN_GL','LEARN_SMOKE']}
    cpu={**env,'SDL_VIDEODRIVER':'unavailable','DISPLAY':'','WAYLAND_DISPLAY':''}
    expected='score=1000 gain=1000 lines=4 level=1 interval=1\nscore=2000 gain=1000 lines=8 level=1 interval=1\nscore=3000 gain=1000 lines=12 level=2 interval=29\nscore=5000 gain=2000 lines=16 level=2 interval=29'
    for backend in ['SCRIPTED','SDL']:
        b=OUT/backend.lower()
        run(['cmake','-S',str(SOURCE),'-B',str(b),'-DSTUDY_PLATFORM='+backend,'-DCMAKE_BUILD_TYPE=Release','-DCMAKE_CXX_FLAGS=-Wall -Wextra -Wpedantic'])
        r=run(['cmake','--build',str(b),'-j3'],timeout=180);assert 'warning:' not in r.stderr,r.stderr
        print(run(['ctest','--test-dir',str(b),'--output-on-failure']).stdout.strip(),flush=True)
        for name in ['score_demo','score_contract']:
            link=(b/'CMakeFiles'/f'{name}.dir/link.txt').read_text()
            assert all(x not in link for x in ['SDL','study_gl','study_platform']),link
        demo=run([str(b/'score_demo')],env=cpu).stdout.strip();assert demo==expected,demo;print(demo,flush=True)
        if backend=='SDL':
            assert 'four-clears' in run([str(b/'tetris'),'--help'],env=cpu).stdout
            r=subprocess.run([str(b/'tetris'),'O','four-clears'],env=cpu,text=True,capture_output=True,timeout=5)
            assert r.returncode==2 and 'Usage:' in r.stdout
            r=run([str(b/'score_probe'),str(OUT/'frame')],env={**env,'SDL_VIDEODRIVER':'offscreen'})
            (OUT/'gl.log').write_text(r.stdout);assert r.stdout.count('all framebuffer RGB matched')==42
            print('42 GL frames: five real game states plus nine numeric cases at three sizes, complete VBO and all RGB pixels matched',flush=True)
    flags=['-std=c++17','-O1','-DNDEBUG','-fsanitize=undefined','-fno-sanitize-recover=all']
    run(['c++',*flags,'-I'+str(SOURCE),str(SOURCE/'tests/score_contract.cpp'),'-o',str(OUT/'sanitized')])
    print(run([str(OUT/'sanitized')]).stdout.strip(),flush=True)
    run(['c++',*flags,'-I'+str(ROOT),str(ROOT/'tests/sim_score_test.cpp'),str(ROOT/'src/sim_game.cpp'),str(ROOT/'src/position.cpp'),'-o',str(OUT/'current_score')])
    print(run([str(OUT/'current_score')]).stdout.strip(),flush=True)
    m=OUT/'mutation';(m/'simulation').mkdir(parents=True,exist_ok=True)
    s=(SOURCE/'simulation/round.h').read_text();needle='candidate.totals_ = *scored;';assert needle in s
    (m/'simulation/round.h').write_text(s.replace(needle,'/* missing score commit */'))
    run(['c++','-std=c++17','-O1','-DNDEBUG','-I'+str(m),'-I'+str(SOURCE),str(SOURCE/'tests/score_contract.cpp'),'-o',str(OUT/'missing_score')])
    r=subprocess.run([str(OUT/'missing_score')],text=True,capture_output=True,timeout=5)
    assert r.returncode==1 and 'score line' in r.stderr,r
    print('Missing-score-commit mutation rejected in Release.',flush=True)
if __name__=='__main__':main()
