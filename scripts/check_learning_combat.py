"""Check attack deltas, pending batches, overflow and GPU output."""
from pathlib import Path
import os, subprocess
ROOT=Path(__file__).resolve().parents[1]
SOURCE=ROOT/'docs/learn/checkpoints/42-combat'
OUT=ROOT/'out/learning-checkpoints/42-combat-check'
def run(args,**kw):
    r=subprocess.run(args,cwd=ROOT,text=True,capture_output=True,timeout=kw.pop('timeout',60),**kw)
    if r.returncode:raise RuntimeError(f'{args}\n{r.stdout}\n{r.stderr}')
    return r

def main():
    OUT.mkdir(parents=True,exist_ok=True)
    env={k:v for k,v in os.environ.items() if k not in ['LD_PRELOAD','LEARN_GL','LEARN_SMOKE']}
    cpu={**env,'SDL_VIDEODRIVER':'unavailable','DISPLAY':'','WAYLAND_DISPLAY':''}
    expected='tick=1 sentL=1 sentR=0 pendingR=1 insertedR=0 holeCursorR=0\ntick=2 sentL=0 sentR=0 pendingR=0 insertedR=1 holeCursorR=1'
    for backend in ['SCRIPTED','SDL']:
        build=OUT/backend.lower()
        run(['cmake','-S',str(SOURCE),'-B',str(build),'-DSTUDY_PLATFORM='+backend,'-DCMAKE_BUILD_TYPE=Release','-DCMAKE_CXX_FLAGS=-Wall -Wextra -Wpedantic'])
        r=run(['cmake','--build',str(build),'-j3'],timeout=240);assert 'warning:' not in r.stderr,r.stderr
        (OUT/f'build-{backend.lower()}.log').write_text(r.stdout+r.stderr)
        r=run(['ctest','--test-dir',str(build),'--output-on-failure']);(OUT/f'ctest-{backend.lower()}.log').write_text(r.stdout)
        print(r.stdout.strip(),flush=True)
        for name in ['combat_demo','combat_contract']:
            link=(build/'CMakeFiles'/f'{name}.dir/link.txt').read_text()
            assert all(dep not in link for dep in ['SDL','study_gl','study_platform']),link
        demo=run([str(build/'combat_demo')],env=cpu).stdout.strip();assert demo==expected,demo
        print(demo,flush=True)
        if backend=='SDL':
            r=run([str(build/'combat_probe'),str(OUT/'frame')],env={**env,'SDL_VIDEODRIVER':'offscreen'},timeout=120)
            (OUT/'gl.log').write_text(r.stdout);assert r.stdout.count('all framebuffer RGB matched')==210
            print('210 GL frames: seven kinds x five garbage cases x before/after x three sizes; full board/active/ghost VBO and all RGB matched.',flush=True)
    flags=['-std=c++17','-O1','-DNDEBUG','-fsanitize=undefined','-fno-sanitize-recover=all']
    run(['c++',*flags,'-I'+str(SOURCE),str(SOURCE/'tests/combat_contract.cpp'),'-o',str(OUT/'sanitized')])
    print(run([str(OUT/'sanitized')]).stdout.strip(),flush=True)
    run(['c++',*flags,'-I'+str(ROOT),str(ROOT/'tests/sim_combat_test.cpp'),str(ROOT/'src/sim_game.cpp'),str(ROOT/'src/position.cpp'),'-o',str(OUT/'current_sanitized')])
    print(run([str(OUT/'current_sanitized')]).stdout.strip(),flush=True)
    for name,file,old,new in [('self_attack','duel.h','!candidate.right_.add_garbage(static_cast<int>(ld','!candidate.left_.add_garbage(static_cast<int>(ld'),('missing_shift','combat.h','const int source = r + rows;','const int source = r;'),('ignored_overflow','combat.h','result.overflow = true;','result.overflow = false;')]:
        mutation=OUT/name;(mutation/'simulation').mkdir(parents=True,exist_ok=True)
        original=(SOURCE/'simulation'/file).read_text();assert old in original
        (mutation/'simulation'/file).write_text(original.replace(old,new))
        run(['c++','-std=c++17','-O1','-DNDEBUG','-I'+str(mutation),'-I'+str(SOURCE),str(SOURCE/'tests/combat_contract.cpp'),'-o',str(mutation/'check')])
        r=subprocess.run([str(mutation/'check')],text=True,capture_output=True,timeout=5)
        assert r.returncode==1 and 'combat line' in r.stderr
    print('Self-attack, missing shift and ignored overflow mutations rejected in Release.',flush=True)
if __name__=='__main__':main()
