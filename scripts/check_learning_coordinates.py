"""Validate CPU coordinate contracts against actual GL framebuffer observations."""
from pathlib import Path
import json
import os
import subprocess
ROOT=Path(__file__).resolve().parents[1]
SOURCE=ROOT/'docs/learn/checkpoints/19-coordinates'
OUT=ROOT/'out/learning-checkpoints/19-coordinates-check'
def run(args,**kwargs):
    r=subprocess.run(args,cwd=ROOT,text=True,capture_output=True,timeout=60,**kwargs)
    if r.returncode:raise RuntimeError(f'{args}\n{r.stdout}\n{r.stderr}')
    return r

def main():
    for backend in ['SCRIPTED','SDL']:
        build=OUT/backend.lower()
        run(['cmake','-S',str(SOURCE),'-B',str(build),'-DSTUDY_PLATFORM='+backend,
             '-DCMAKE_BUILD_TYPE=Release','-DCMAKE_EXPORT_COMPILE_COMMANDS=ON',
             '-DCMAKE_CXX_FLAGS=-Wall -Wextra -Wpedantic'])
        r=run(['cmake','--build',str(build),'-j2']);assert 'warning:' not in r.stderr,r.stderr
        print(run(['ctest','--test-dir',str(build),'--output-on-failure']).stdout.strip())
        for row in json.loads((build/'compile_commands.json').read_text()):
            if '/renderer/' in row['file'] or 'coordinates_demo' in row['file']:
                assert 'SDL2' not in row['command'],row
        for name in ['base','w2','scaled','oversized']:
            print(run([str(build/'coordinates_demo'),name]).stdout.strip())
        for args in [['missing'],['base','extra']]:
            r=subprocess.run([str(build/'coordinates_demo'),*args],capture_output=True,text=True,timeout=5)
            assert r.returncode==2 and 'Usage:' in r.stderr
            if backend=='SDL':
                r=subprocess.run([str(build/'tetris'),*args],capture_output=True,text=True,timeout=5,
                                 env={**os.environ,'SDL_VIDEODRIVER':'unavailable'})
                assert r.returncode==2 and 'Usage:' in r.stderr
        if backend=='SDL':
            env={k:v for k,v in os.environ.items() if k not in ['LD_PRELOAD','LEARN_GL','LEARN_SMOKE']}
            print(run([str(build/'coordinates_probe'),str(OUT/'coordinates')],
                      env={**env,'SDL_VIDEODRIVER':'offscreen'}).stdout.strip())
    # Negative control: omitting the homogeneous divide must be caught even in Release.
    mutation=OUT/'mutation';(mutation/'renderer').mkdir(parents=True,exist_ok=True)
    for name in ['projection_cases.h','shader_sources.h']:
        (mutation/'renderer'/name).write_text((SOURCE/'renderer'/name).read_text())
    h=(SOURCE/'renderer/coordinates.h').read_text()
    assert 'p.x / p.w, p.y / p.w, p.z / p.w' in h
    (mutation/'renderer/coordinates.h').write_text(h.replace('p.x / p.w, p.y / p.w, p.z / p.w','p.x, p.y, p.z'))
    run(['c++','-std=c++17','-DNDEBUG','-I'+str(mutation),'-I'+str(SOURCE),
         str(SOURCE/'tests/coordinates_contract.cpp'),'-o',str(OUT/'missing_divide')])
    r=subprocess.run([str(OUT/'missing_divide')],capture_output=True,text=True,timeout=5)
    assert r.returncode==1 and 'line ' in r.stderr
    print('Missing-w-divide negative control rejected; native presentation/physical GPU/other OS not claimed.')
if __name__=='__main__':main()
