"""Compare a teaching triangle sampler with GL interpolation and discard."""
from pathlib import Path
import json
import os
import subprocess
ROOT=Path(__file__).resolve().parents[1]
SOURCE=ROOT/'docs/learn/checkpoints/20-raster'
OUT=ROOT/'out/learning-checkpoints/20-raster-check'
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
            if '/renderer/' in row['file'] or 'raster_demo' in row['file']:
                assert 'SDL2' not in row['command'],row
        output=run([str(build/'raster_demo')]).stdout
        assert output.count('#')==33 and '(0.375,0.375,0.250)' in output,output
        print(output.strip())
        if backend=='SDL':
            env={k:v for k,v in os.environ.items() if k not in ['LD_PRELOAD','LEARN_GL','LEARN_SMOKE']}
            for args in [['missing'],['smooth','extra']]:
                r=subprocess.run([str(build/'tetris'),*args],capture_output=True,text=True,timeout=5,
                                 env={**env,'SDL_VIDEODRIVER':'unavailable'})
                assert r.returncode==2 and 'Usage:' in r.stderr
            print(run([str(build/'raster_probe'),str(OUT/'raster')],
                      env={**env,'SDL_VIDEODRIVER':'offscreen'}).stdout.strip())
    # A missing half-pixel offset is a real prediction bug, caught without NDEBUG asserts.
    mutation=OUT/'mutation';(mutation/'renderer').mkdir(parents=True,exist_ok=True)
    h=(SOURCE/'renderer/raster.h').read_text()
    assert 'static_cast<double>(x) + 0.5' in h
    (mutation/'renderer/raster.h').write_text(h.replace('static_cast<double>(x) + 0.5','static_cast<double>(x)'))
    run(['c++','-std=c++17','-DNDEBUG','-I'+str(mutation),
         str(SOURCE/'tests/raster_contract.cpp'),'-o',str(OUT/'wrong_center')])
    r=subprocess.run([str(OUT/'wrong_center')],capture_output=True,text=True,timeout=5)
    assert r.returncode==1 and 'p.x==1.5' in r.stderr
    print('Integer-corner negative control rejected; no physical GPU or native presentation claim.')
if __name__=='__main__':main()
