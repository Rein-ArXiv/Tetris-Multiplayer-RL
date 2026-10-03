"""Check next FIFO ownership and preview updates and framebuffer output."""
from pathlib import Path
import os, subprocess
ROOT=Path(__file__).resolve().parents[1]
SOURCE=ROOT/'docs/learn/checkpoints/36-next-queue'
OUT=ROOT/'out/learning-checkpoints/36-next-queue-check'
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
        for name in ['queue_demo','queue_contract']:
            link=(b/'CMakeFiles'/f'{name}.dir/link.txt').read_text()
            assert all(x not in link for x in ['SDL','study_gl','study_platform']),link
        out=run([str(b/'queue_demo')],env=cpuenv).stdout.strip()
        expected='current=O next=S T Z cursor=4\ncurrent=S next=T Z I cursor=5\ncurrent=T next=Z I J cursor=6\ncaller source cursor=0'
        assert out==expected,out;print(out,flush=True)
        if backend=='SDL':
            assert 'Usage:' in run([str(b/'tetris'),'--help'],env=cpuenv).stdout
            print(run([str(b/'queue_probe'),str(OUT/'frame')],env={**env,'SDL_VIDEODRIVER':'offscreen'}).stdout.strip(),flush=True)
            print(run([str(b/'rotation_input')],env={**env,'SDL_VIDEODRIVER':'dummy'}).stdout.strip(),flush=True)
    flags=['-std=c++17','-O1','-DNDEBUG','-fsanitize=undefined','-fno-sanitize-recover=all']
    run(['c++',*flags,'-I'+str(SOURCE),str(SOURCE/'tests/queue_contract.cpp'),'-o',str(OUT/'sanitized')])
    print(run([str(OUT/'sanitized')]).stdout.strip(),flush=True)
    run(['c++',*flags,'-I'+str(ROOT),'-I'+str(SOURCE),str(ROOT/'tests/learning/current_queue.cpp'),str(ROOT/'src/sim_game.cpp'),str(ROOT/'src/position.cpp'),'-o',str(OUT/'current_queue')])
    print(run([str(OUT/'current_queue')]).stdout.strip(),flush=True)
    m=OUT/'mutation';(m/'simulation').mkdir(parents=True,exist_ok=True)
    s=(SOURCE/'simulation/next_queue.h').read_text();needle='head_ = (head_ + 1) % capacity;';assert needle in s
    (m/'simulation/next_queue.h').write_text(s.replace(needle,'head_ = head_;'))
    run(['c++','-std=c++17','-O1','-DNDEBUG','-I'+str(m),'-I'+str(SOURCE),str(SOURCE/'tests/queue_contract.cpp'),'-o',str(OUT/'stuck_head')])
    r=subprocess.run([str(OUT/'stuck_head')],text=True,capture_output=True,timeout=5)
    assert r.returncode==1 and 'queue line' in r.stderr,r
    print('Stuck-head mutation rejected in Release; production preview contract and whole-frame preview pixels passed.',flush=True)
if __name__=='__main__':main()
