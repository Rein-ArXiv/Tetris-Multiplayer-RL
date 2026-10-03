"""Check one-shot hard drops, commit boundaries, board transitions and GPU output."""
from pathlib import Path
import os, subprocess
ROOT=Path(__file__).resolve().parents[1]
SOURCE=ROOT/'docs/learn/checkpoints/41-hard-drop'
OUT=ROOT/'out/learning-checkpoints/41-hard-drop-check'
def run(args,**kw):
    r=subprocess.run(args,cwd=ROOT,text=True,capture_output=True,timeout=kw.pop('timeout',60),**kw)
    if r.returncode:raise RuntimeError(f'{args}\n{r.stdout}\n{r.stderr}')
    return r

def main():
    OUT.mkdir(parents=True,exist_ok=True)
    env={k:v for k,v in os.environ.items() if k not in ['LD_PRELOAD','LEARN_GL','LEARN_SMOKE']}
    cpu={**env,'SDL_VIDEODRIVER':'unavailable','DISPLAY':'','WAYLAND_DISPLAY':''}
    expected='\n'.join(f'tick={n} drop={int(n==1)} distance={18 if n==1 else -1} filled=4 row=0 score=0' for n in range(1,4))
    for backend in ['SCRIPTED','SDL']:
        build=OUT/backend.lower()
        run(['cmake','-S',str(SOURCE),'-B',str(build),'-DSTUDY_PLATFORM='+backend,'-DCMAKE_BUILD_TYPE=Release','-DCMAKE_CXX_FLAGS=-Wall -Wextra -Wpedantic'])
        r=run(['cmake','--build',str(build),'-j3'],timeout=240);assert 'warning:' not in r.stderr,r.stderr
        (OUT/f'build-{backend.lower()}.log').write_text(r.stdout+r.stderr)
        r=run(['ctest','--test-dir',str(build),'--output-on-failure']);(OUT/f'ctest-{backend.lower()}.log').write_text(r.stdout)
        print(r.stdout.strip(),flush=True)
        for name in ['hard_drop_demo','hard_drop_contract']:
            link=(build/'CMakeFiles'/f'{name}.dir/link.txt').read_text()
            assert all(dep not in link for dep in ['SDL','study_gl','study_platform']),link
        demo=run([str(build/'hard_drop_demo')],env=cpu).stdout.strip();assert demo==expected,demo
        print(demo,flush=True)
        if backend=='SDL':
            r=run([str(build/'hard_drop_probe'),str(OUT/'frame')],env={**env,'SDL_VIDEODRIVER':'offscreen'},timeout=120)
            (OUT/'gl.log').write_text(r.stdout);assert r.stdout.count('all framebuffer RGB matched')==168
            print('168 GL frames: seven kinds x four boards x before/after x three sizes; full board/active/ghost VBO and all RGB matched.',flush=True)
    flags=['-std=c++17','-O1','-DNDEBUG','-fsanitize=undefined','-fno-sanitize-recover=all']
    run(['c++',*flags,'-I'+str(SOURCE),str(SOURCE/'tests/hard_drop_contract.cpp'),'-o',str(OUT/'sanitized')])
    print(run([str(OUT/'sanitized')]).stdout.strip(),flush=True)
    run(['c++',*flags,'-I'+str(ROOT),str(ROOT/'tests/sim_hard_drop_test.cpp'),str(ROOT/'src/sim_game.cpp'),str(ROOT/'src/position.cpp'),'-o',str(OUT/'current_sanitized')])
    print(run([str(OUT/'current_sanitized')]).stdout.strip(),flush=True)
    for name,file,old,new in [('missing_landing','round.h','            candidate.active_ = landing->piece;','            // incorrect: omit landing assignment'),('replayed_edge','pending_controls.h','        hard_drop_ = false;','        // incorrect: keep consumed request')]:
        mutation=OUT/name;(mutation/'simulation').mkdir(parents=True,exist_ok=True)
        original=(SOURCE/'simulation'/file).read_text();assert old in original
        (mutation/'simulation'/file).write_text(original.replace(old,new))
        run(['c++','-std=c++17','-O1','-DNDEBUG','-I'+str(mutation),'-I'+str(SOURCE),str(SOURCE/'tests/hard_drop_contract.cpp'),'-o',str(mutation/'check')])
        r=subprocess.run([str(mutation/'check')],text=True,capture_output=True,timeout=5)
        assert r.returncode==1 and 'hard drop line' in r.stderr
    print('Missing landing and replayed edge mutations rejected in Release.',flush=True)
if __name__=='__main__':main()
