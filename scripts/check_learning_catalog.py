"""Check catalog identities, all 4-cell connectivity sets, CLI and actual GL output."""
from pathlib import Path
import json,os,subprocess
ROOT=Path(__file__).resolve().parents[1]
SOURCE=ROOT/'docs/learn/checkpoints/28-piece-catalog'
OUT=ROOT/'out/learning-checkpoints/28-piece-catalog-check'
def run(args,**kw):
    r=subprocess.run(args,cwd=ROOT,text=True,capture_output=True,timeout=60,**kw)
    if r.returncode:raise RuntimeError(f'{args}\n{r.stdout}\n{r.stderr}')
    return r

def main():
    env={k:v for k,v in os.environ.items() if k not in ['LD_PRELOAD','LEARN_GL','LEARN_SMOKE']}
    cpuenv={**env,'SDL_VIDEODRIVER':'unavailable','DISPLAY':'','WAYLAND_DISPLAY':''}
    for backend in ['SCRIPTED','SDL']:
        b=OUT/backend.lower()
        run(['cmake','-S',str(SOURCE),'-B',str(b),'-DSTUDY_PLATFORM='+backend,'-DCMAKE_BUILD_TYPE=Release','-DCMAKE_EXPORT_COMPILE_COMMANDS=ON','-DCMAKE_CXX_FLAGS=-Wall -Wextra -Wpedantic'])
        r=run(['cmake','--build',str(b),'-j3']);assert 'warning:' not in r.stderr,r.stderr
        print(run(['ctest','--test-dir',str(b),'--output-on-failure']).stdout.strip())
        for row in json.loads((b/'compile_commands.json').read_text()):
            if 'catalog_demo' in row['file'] or 'catalog_contract' in row['file']:assert 'SDL2' not in row['command'],row
        for name in ['catalog_demo','catalog_contract']:
            link=(b/'CMakeFiles'/f'{name}.dir/link.txt').read_text()
            assert all(x not in link for x in ['SDL','study_gl','study_platform']),link
        out=run([str(b/'catalog_demo')],env=cpuenv).stdout
        lines=out.splitlines();ids=[3,2,1,4,5,6,7];names=['I','J','L','O','S','T','Z'];masks=[0xF0,0x71,0x74,0x33,0x36,0x72,0x63]
        for i in range(7):
            assert lines[i*5]==f'index={i} name={names[i]} id={ids[i]} spawn=(0,{4 if i==3 else 3}) visible=4'
            expected=[''.join('#' if masks[i]&(1<<(r*4+c)) else '.' for c in range(4)) for r in range(4)]
            assert lines[i*5+1:i*5+5]==expected
        assert lines[-1]=='raw id 257: found=0';print(out.strip())
        if backend=='SDL':
            assert 'Usage:' in run([str(b/'tetris'),'--help'],env=cpuenv).stdout
            for args in [['X'],['t'],['T '],['257'],['I','J']]:
                r=subprocess.run([str(b/'tetris'),*args],cwd=ROOT,env=cpuenv,text=True,capture_output=True,timeout=5)
                assert r.returncode==2 and 'Usage:' in r.stdout and 'selected=' not in r.stdout,r
            print(run([str(b/'catalog_probe'),str(OUT/'frame')],env={**env,'SDL_VIDEODRIVER':'offscreen'}).stdout.strip())
    flags=['-std=c++17','-DNDEBUG','-fsanitize=undefined','-fno-sanitize-recover=all']
    run(['c++',*flags,'-I'+str(SOURCE),str(SOURCE/'tests/catalog_contract.cpp'),'-o',str(OUT/'sanitized')]);print(run([str(OUT/'sanitized')]).stdout.strip())
    run(['c++',*flags,'-I'+str(ROOT),'-I'+str(SOURCE),str(ROOT/'tests/learning/current_catalog.cpp'),str(ROOT/'src/position.cpp'),'-o',str(OUT/'current_catalog')]);print(run([str(OUT/'current_catalog')]).stdout.strip())
    m=OUT/'mutation';(m/'simulation').mkdir(parents=True,exist_ok=True)
    s=(SOURCE/'simulation/catalog.h').read_text();needle='adr + adc == 1';assert needle in s
    (m/'simulation/catalog.h').write_text(s.replace(needle,'adr <= 1 && adc <= 1'))
    run(['c++','-std=c++17','-DNDEBUG','-I'+str(m),'-I'+str(SOURCE),str(SOURCE/'tests/catalog_contract.cpp'),'-o',str(OUT/'diagonal')])
    r=subprocess.run([str(OUT/'diagonal')],text=True,capture_output=True,timeout=5)
    assert r.returncode==1 and 'catalog line' in r.stderr,r
    print('Diagonal-connectivity negative control rejected in Release; current spawn data and CLI boundaries verified.')
if __name__=='__main__':main()
