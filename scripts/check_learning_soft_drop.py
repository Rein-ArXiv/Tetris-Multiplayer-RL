"""Verify held sampling, repeat deadlines, integrated falls and actual GL output."""
from pathlib import Path
import os, subprocess
ROOT=Path(__file__).resolve().parents[1]
SOURCE=ROOT/'docs/learn/checkpoints/40-soft-drop'
OUT=ROOT/'out/learning-checkpoints/40-soft-drop-check'
def run(args,**kw):
    r=subprocess.run(args,cwd=ROOT,text=True,capture_output=True,timeout=kw.pop('timeout',60),**kw)
    if r.returncode:raise RuntimeError(f'{args}\n{r.stdout}\n{r.stderr}')
    return r

def main():
    OUT.mkdir(parents=True,exist_ok=True)
    env={k:v for k,v in os.environ.items() if k not in ['LD_PRELOAD','LEARN_GL','LEARN_SMOKE']}
    cpu={**env,'SDL_VIDEODRIVER':'unavailable','DISPLAY':'','WAYLAND_DISPLAY':''}
    rows=[1,1,1,2,2,3,3,4,4,5,5,6];waits=[3,2,1,0,0,3,2,1,0,3,2,1]
    expected='\n'.join(f'tick={n} down={int(n not in [4,5])} row={rows[n-1]} wait={waits[n-1]} gravity={n%4}' for n in range(1,13))
    for backend in ['SCRIPTED','SDL']:
        build=OUT/backend.lower()
        run(['cmake','-S',str(SOURCE),'-B',str(build),'-DSTUDY_PLATFORM='+backend,'-DCMAKE_BUILD_TYPE=Release','-DCMAKE_CXX_FLAGS=-Wall -Wextra -Wpedantic'])
        r=run(['cmake','--build',str(build),'-j3'],timeout=240);assert 'warning:' not in r.stderr,r.stderr
        print(run(['ctest','--test-dir',str(build),'--output-on-failure']).stdout.strip(),flush=True)
        for name in ['soft_drop_demo','soft_drop_contract']:
            link=(build/'CMakeFiles'/f'{name}.dir/link.txt').read_text()
            assert all(dep not in link for dep in ['SDL','study_gl','study_platform']),link
        demo=run([str(build/'soft_drop_demo')],env=cpu).stdout.strip();assert demo==expected,demo
        print(demo,flush=True)
        if backend=='SDL':
            r=run([str(build/'soft_drop_probe'),str(OUT/'frame')],env={**env,'SDL_VIDEODRIVER':'offscreen'},timeout=120)
            (OUT/'gl.log').write_text(r.stdout);assert r.stdout.count('all framebuffer RGB matched')==105
            print('105 GL frames: seven kinds x five actual soft/gravity states x three sizes. Active/ghost VBO and every RGB pixel matched.',flush=True)
    flags=['-std=c++17','-O1','-DNDEBUG','-fsanitize=undefined','-fno-sanitize-recover=all']
    run(['c++',*flags,'-I'+str(SOURCE),str(SOURCE/'tests/soft_drop_contract.cpp'),'-o',str(OUT/'sanitized')])
    print(run([str(OUT/'sanitized')]).stdout.strip(),flush=True)
    run(['c++',*flags,'-I'+str(ROOT),str(ROOT/'tests/sim_soft_drop_test.cpp'),str(ROOT/'src/sim_game.cpp'),str(ROOT/'src/position.cpp'),'-o',str(OUT/'current_sanitized')])
    print(run([str(OUT/'current_sanitized')]).stdout.strip(),flush=True)
    mutation=OUT/'mutation';(mutation/'simulation').mkdir(parents=True,exist_ok=True)
    original=(SOURCE/'simulation/soft_drop.h').read_text();assert 'c.remaining = c.period - 1;' in original
    (mutation/'simulation/soft_drop.h').write_text(original.replace('c.remaining = c.period - 1;','c.remaining = c.period;'))
    run(['c++','-std=c++17','-O1','-DNDEBUG','-I'+str(mutation),'-I'+str(SOURCE),str(SOURCE/'tests/soft_drop_contract.cpp'),'-o',str(OUT/'wrong_period')])
    r=subprocess.run([str(OUT/'wrong_period')],text=True,capture_output=True,timeout=5)
    assert r.returncode==1 and 'soft drop line' in r.stderr
    print('Wrong-period reload mutation rejected in Release.',flush=True)
if __name__=='__main__':main()
