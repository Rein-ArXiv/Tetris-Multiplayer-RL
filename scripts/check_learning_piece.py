"""Check pure block placement, current value ownership and the composed GL frame."""
from pathlib import Path
import json,os,subprocess
ROOT=Path(__file__).resolve().parents[1]
SOURCE=ROOT/'docs/learn/checkpoints/27-local-piece'
OUT=ROOT/'out/learning-checkpoints/27-local-piece-check'
def run(args,**kw):
    r=subprocess.run(args,cwd=ROOT,text=True,capture_output=True,timeout=60,**kw)
    if r.returncode:raise RuntimeError(f'{args}\n{r.stdout}\n{r.stderr}')
    return r

def main():
    env={k:v for k,v in os.environ.items() if k not in ['LD_PRELOAD','LEARN_GL','LEARN_SMOKE']}
    for backend in ['SCRIPTED','SDL']:
        b=OUT/backend.lower()
        run(['cmake','-S',str(SOURCE),'-B',str(b),'-DSTUDY_PLATFORM='+backend,'-DCMAKE_BUILD_TYPE=Release','-DCMAKE_EXPORT_COMPILE_COMMANDS=ON','-DCMAKE_CXX_FLAGS=-Wall -Wextra -Wpedantic'])
        r=run(['cmake','--build',str(b),'-j3']);assert 'warning:' not in r.stderr,r.stderr
        print(run(['ctest','--test-dir',str(b),'--output-on-failure']).stdout.strip())
        for row in json.loads((b/'compile_commands.json').read_text()):
            if 'piece_demo' in row['file'] or 'piece_contract' in row['file']:assert 'SDL2' not in row['command'],row
        for name in ['piece_demo','piece_contract']:
            link=(b/'CMakeFiles'/f'{name}.dir/link.txt').read_text()
            assert all(x not in link for x in ['SDL','study_gl','study_platform']),link
        out=run([str(b/'piece_demo')],env={**env,'SDL_VIDEODRIVER':'unavailable','DISPLAY':'','WAYLAND_DISPLAY':''}).stdout
        for line in ['origin=(4,3): (4,4) (5,3) (5,4) (5,5) visible=4 vertices=24',
                     'origin=(-1,3): (-1,4) (0,3) (0,4) (0,5) visible=3 vertices=18',
                     'origin=(-4,3): (-4,4) (-3,3) (-3,4) (-3,5) visible=0 vertices=0',
                     'origin=(19,8): (19,9) (20,8) (20,9) (20,10) visible=1 vertices=6','arithmetic failure']:
            assert line in out,(line,out)
        print(out.strip())
        if backend=='SDL':print(run([str(b/'piece_probe'),str(OUT/'frame')],env={**env,'SDL_VIDEODRIVER':'offscreen'}).stdout.strip())
    flags=['-std=c++17','-DNDEBUG','-fsanitize=undefined','-fno-sanitize-recover=all']
    run(['c++',*flags,'-I'+str(SOURCE),str(SOURCE/'tests/piece_contract.cpp'),'-o',str(OUT/'sanitized')])
    print(run([str(OUT/'sanitized')]).stdout.strip())
    run(['c++',*flags,'-I'+str(ROOT),str(ROOT/'tests/learning/current_piece.cpp'),str(ROOT/'src/position.cpp'),'-o',str(OUT/'current_piece')])
    print(run([str(OUT/'current_piece')]).stdout.strip())
    m=OUT/'mutation';(m/'simulation').mkdir(parents=True,exist_ok=True)
    s=(SOURCE/'simulation/piece.h').read_text()
    needle='static_cast<std::int64_t>(piece.local[i].row) +\n            static_cast<std::int64_t>(piece.origin.row)';assert needle in s
    (m/'simulation/piece.h').write_text(s.replace(needle,'static_cast<std::int64_t>(piece.local[i].row + piece.origin.row)'))
    run(['c++',*flags,'-I'+str(m),'-I'+str(SOURCE),str(SOURCE/'tests/piece_contract.cpp'),'-o',str(OUT/'late_widening')])
    r=subprocess.run([str(OUT/'late_widening')],text=True,capture_output=True,timeout=5)
    assert r.returncode!=0 and 'signed integer overflow' in r.stderr,r
    print('Addition-before-widening mutation rejected by UBSan; no shape or collision validity inferred from translation.')
if __name__=='__main__':main()
