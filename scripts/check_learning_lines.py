"""Check stable row compaction, clear/spawn order and framebuffer output."""
from pathlib import Path
import os, subprocess
ROOT=Path(__file__).resolve().parents[1]
SOURCE=ROOT/'docs/learn/checkpoints/33-line-clear'
OUT=ROOT/'out/learning-checkpoints/33-line-clear-check'
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
        for name in ['lines_demo','lines_contract']:
            link=(b/'CMakeFiles'/f'{name}.dir/link.txt').read_text()
            assert all(x not in link for x in ['SDL','study_gl','study_platform']),link
        out=run([str(b/'lines_demo')],env=cpuenv).stdout.strip()
        expected='before: filled=18\ntick=570 cleared=2 filled=2 active_row=0\nmarkers: (7,0)=1 (14,9)=1'
        assert out==expected,out;print(out,flush=True)
        if backend=='SDL':
            assert 'Usage:' in run([str(b/'tetris'),'--help'],env=cpuenv).stdout
            print(run([str(b/'lines_probe'),str(OUT/'frame')],env={**env,'SDL_VIDEODRIVER':'offscreen'}).stdout.strip(),flush=True)
    flags=['-std=c++17','-O1','-DNDEBUG','-fsanitize=undefined','-fno-sanitize-recover=all']
    run(['c++',*flags,'-I'+str(SOURCE),str(SOURCE/'tests/lines_contract.cpp'),'-o',str(OUT/'sanitized')])
    print(run([str(OUT/'sanitized')]).stdout.strip(),flush=True)
    run(['c++',*flags,'-I'+str(ROOT),'-I'+str(SOURCE),str(ROOT/'tests/learning/current_lines.cpp'),str(ROOT/'src/sim_game.cpp'),str(ROOT/'src/position.cpp'),'-o',str(OUT/'current_lines')])
    print(run([str(OUT/'current_lines')]).stdout.strip(),flush=True)
    m=OUT/'mutation';(m/'simulation').mkdir(parents=True,exist_ok=True)
    s=(SOURCE/'simulation/lines.h').read_text();needle='for (int row = 0; row <= write; ++row)';assert needle in s
    (m/'simulation/lines.h').write_text(s.replace(needle,'for (int row = 0; false && row <= write; ++row)'))
    run(['c++','-std=c++17','-O1','-DNDEBUG','-I'+str(m),'-I'+str(SOURCE),str(SOURCE/'tests/lines_contract.cpp'),'-o',str(OUT/'stale_top')])
    r=subprocess.run([str(OUT/'stale_top')],text=True,capture_output=True,timeout=5)
    assert r.returncode==1 and 'lines line' in r.stderr,r
    print('Missing-top-clear mutation rejected in Release; production row parity and clear/spawn pixels passed.',flush=True)
if __name__=='__main__':main()
