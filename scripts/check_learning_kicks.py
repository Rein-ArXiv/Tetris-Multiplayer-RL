"""Check ordered kick candidates and framebuffer output."""
from pathlib import Path
import os, subprocess
ROOT=Path(__file__).resolve().parents[1]
SOURCE=ROOT/'docs/learn/checkpoints/35-kicks'
OUT=ROOT/'out/learning-checkpoints/35-kicks-check'
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
        for name in ['kicks_demo','kicks_contract']:
            link=(b/'CMakeFiles'/f'{name}.dir/link.txt').read_text()
            assert all(x not in link for x in ['SDL','study_gl','study_platform']),link
        out=run([str(b/'kicks_demo')],env=cpuenv).stdout.strip()
        expected='case=0 rotated candidate=1 quarter=2 origin=(5,0)\ncase=1 rotated candidate=5 quarter=1 origin=(17,3)\ncase=2 rotated candidate=6 quarter=1 origin=(16,3)\ncase=3 rotated candidate=1 quarter=1 origin=(5,2)\ncase=4 rotated candidate=2 quarter=1 origin=(5,4)\ncase=5 blocked candidate=-1 quarter=0 origin=(5,3)\ncase=6 rotated candidate=0 quarter=1 origin=(5,3)'
        assert out==expected,out;print(out,flush=True)
        if backend=='SDL':
            assert 'Usage:' in run([str(b/'tetris'),'--help'],env=cpuenv).stdout
            print(run([str(b/'kicks_probe'),str(OUT/'frame')],env={**env,'SDL_VIDEODRIVER':'offscreen'}).stdout.strip(),flush=True)
            print(run([str(b/'rotation_input')],env={**env,'SDL_VIDEODRIVER':'dummy'}).stdout.strip(),flush=True)
    flags=['-std=c++17','-O1','-DNDEBUG','-fsanitize=undefined','-fno-sanitize-recover=all']
    run(['c++',*flags,'-I'+str(SOURCE),str(SOURCE/'tests/kicks_contract.cpp'),'-o',str(OUT/'sanitized')])
    print(run([str(OUT/'sanitized')]).stdout.strip(),flush=True)
    run(['c++',*flags,'-I'+str(ROOT),'-I'+str(SOURCE),str(ROOT/'tests/learning/current_kicks.cpp'),str(ROOT/'src/sim_game.cpp'),str(ROOT/'src/position.cpp'),'-o',str(OUT/'current_kicks')])
    print(run([str(OUT/'current_kicks')]).stdout.strip(),flush=True)
    m=OUT/'mutation';(m/'simulation').mkdir(parents=True,exist_ok=True)
    s=(SOURCE/'simulation/kicks.h').read_text();needle='(quarter == 0 || quarter == 3) ? -1 : 1';assert needle in s
    (m/'simulation/kicks.h').write_text(s.replace(needle,'(quarter == 0 || quarter == 3) ? 1 : -1'))
    run(['c++','-std=c++17','-O1','-DNDEBUG','-I'+str(m),'-I'+str(SOURCE),str(SOURCE/'tests/kicks_contract.cpp'),'-o',str(OUT/'wrong_order')])
    r=subprocess.run([str(OUT/'wrong_order')],text=True,capture_output=True,timeout=5)
    assert r.returncode==1 and 'kicks line' in r.stderr,r
    print('Wrong-priority mutation rejected in Release; production policy difference and kick pixels passed.',flush=True)
if __name__=='__main__':main()
