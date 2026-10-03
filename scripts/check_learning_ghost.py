"""Verify pure landing projection, cache boundaries and the actual ghost overlay."""
from pathlib import Path
import os, subprocess
ROOT=Path(__file__).resolve().parents[1]
SOURCE=ROOT/'docs/learn/checkpoints/39-ghost'
OUT=ROOT/'out/learning-checkpoints/39-ghost-check'
def run(args,**kw):
    r=subprocess.run(args,cwd=ROOT,text=True,capture_output=True,timeout=kw.pop('timeout',60),**kw)
    if r.returncode: raise RuntimeError(f'{args}\n{r.stdout}\n{r.stderr}')
    return r

def main():
    OUT.mkdir(parents=True,exist_ok=True)
    env={k:v for k,v in os.environ.items() if k not in ['LD_PRELOAD','LEARN_GL','LEARN_SMOKE']}
    cpu={**env,'SDL_VIDEODRIVER':'unavailable','DISPLAY':'','WAYLAND_DISPLAY':''}
    for backend in ['SCRIPTED','SDL']:
        build=OUT/backend.lower()
        run(['cmake','-S',str(SOURCE),'-B',str(build),'-DSTUDY_PLATFORM='+backend,'-DCMAKE_BUILD_TYPE=Release','-DCMAKE_CXX_FLAGS=-Wall -Wextra -Wpedantic'])
        r=run(['cmake','--build',str(build),'-j3'],timeout=240)
        assert 'warning:' not in r.stderr,r.stderr
        print(run(['ctest','--test-dir',str(build),'--output-on-failure']).stdout.strip(),flush=True)
        for name in ['ghost_contract','ghost_demo']:
            link=(build/'CMakeFiles'/f'{name}.dir/link.txt').read_text()
            assert all(dep not in link for dep in ['SDL','study_gl','study_platform']),link
        demo=run([str(build/'ghost_demo')],env=cpu).stdout.strip()
        assert demo=='\n'.join(['active=0 ghost=8 distance=8 score=0 elapsed=0']*3),demo
        print(demo,flush=True)
        if backend=='SDL':
            r=run([str(build/'ghost_probe'),str(OUT/'frame')],env={**env,'SDL_VIDEODRIVER':'offscreen'})
            (OUT/'gl.log').write_text(r.stdout)
            assert r.stdout.count('all framebuffer RGB matched')==66
            print('66 GL frames: seven kinds x three poses plus no-overlay, at three sizes. VBO and every RGB pixel matched.',flush=True)
    flags=['-std=c++17','-O1','-DNDEBUG','-fsanitize=undefined','-fno-sanitize-recover=all']
    run(['c++',*flags,'-I'+str(SOURCE),str(SOURCE/'tests/ghost_contract.cpp'),'-o',str(OUT/'sanitized')])
    print(run([str(OUT/'sanitized')]).stdout.strip(),flush=True)
    run(['c++',*flags,'-I'+str(ROOT),str(ROOT/'tests/sim_ghost_test.cpp'),str(ROOT/'src/sim_game.cpp'),str(ROOT/'src/position.cpp'),'-o',str(OUT/'current_ghost')])
    print(run([str(OUT/'current_ghost')]).stdout.strip(),flush=True)
    mutation=OUT/'mutation';(mutation/'simulation').mkdir(parents=True,exist_ok=True)
    s=(SOURCE/'simulation/ghost.h').read_text();assert '++landing.distance;' in s
    (mutation/'simulation/ghost.h').write_text(s.replace('++landing.distance;','/* missing distance accounting */'))
    run(['c++','-std=c++17','-O1','-DNDEBUG','-I'+str(mutation),'-I'+str(SOURCE),str(SOURCE/'tests/ghost_contract.cpp'),'-o',str(OUT/'missing_distance')])
    r=subprocess.run([str(OUT/'missing_distance')],text=True,capture_output=True,timeout=5)
    assert r.returncode==1 and 'ghost line' in r.stderr
    print('Missing-distance mutation rejected in Release.',flush=True)
if __name__=='__main__': main()
