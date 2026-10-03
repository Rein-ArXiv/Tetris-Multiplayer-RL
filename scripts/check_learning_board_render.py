"""Check the board projection, immutable geometry snapshot and actual GL output."""
from pathlib import Path
import json,os,subprocess
ROOT=Path(__file__).resolve().parents[1]
SOURCE=ROOT/'docs/learn/checkpoints/26-board-render'
OUT=ROOT/'out/learning-checkpoints/26-board-render-check'
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
            if 'board_render_demo' in row['file'] or 'board_render_contract' in row['file']:
                assert 'SDL2' not in row['command'],row
        for name in ['board_render_demo','board_render_contract']:
            link=(b/'CMakeFiles'/f'{name}.dir/link.txt').read_text()
            assert all(x not in link for x in ['SDL','study_gl','study_platform']),link
        out=run([str(b/'board_render_demo')],env={**env,'SDL_VIDEODRIVER':'unavailable','DISPLAY':'','WAYLAND_DISPLAY':''}).stdout
        assert '(1,2) -> [130,30)-[139,39)' in out
        assert 'empty=1182 filled=18 total=1200 bytes=9600' in out
        assert 'gap (139.5,39.5) -> row=1 column=2' in out
        print(out.strip())
        if backend=='SDL':
            print(run([str(b/'board_render_probe'),str(OUT/'frame')],env={**env,'SDL_VIDEODRIVER':'offscreen'}).stdout.strip())
    flags=['-std=c++17','-DNDEBUG','-fsanitize=undefined','-fno-sanitize-recover=all']
    run(['c++',*flags,'-I'+str(SOURCE),str(SOURCE/'tests/board_render_contract.cpp'),'-o',str(OUT/'sanitized')])
    print(run([str(OUT/'sanitized')]).stdout.strip())
    m=OUT/'mutation';(m/'renderer').mkdir(parents=True,exist_ok=True)
    s=(SOURCE/'renderer/board_geometry.h').read_text()
    needle='origin_x + static_cast<double>(column) * pitch';assert needle in s
    (m/'renderer/board_geometry.h').write_text(s.replace(needle,'origin_x + static_cast<double>(row) * pitch'))
    run(['c++','-std=c++17','-DNDEBUG','-I'+str(m),'-I'+str(SOURCE),'-I'+str(SOURCE/'renderer'),str(SOURCE/'tests/board_render_contract.cpp'),'-o',str(OUT/'swapped_axis')])
    r=subprocess.run([str(OUT/'swapped_axis')],text=True,capture_output=True,timeout=5)
    assert r.returncode==1 and 'board line' in r.stderr,r
    print('Row/column swap negative control rejected in Release. Actual readback is separate from native presentation.')
if __name__=='__main__':main()
