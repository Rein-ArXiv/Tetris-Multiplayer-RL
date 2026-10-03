"""Validate the pure grid without SDL and its cumulative build integration."""
from pathlib import Path
import json,os,subprocess
ROOT=Path(__file__).resolve().parents[1]
SOURCE=ROOT/'docs/learn/checkpoints/25-grid'
OUT=ROOT/'out/learning-checkpoints/25-grid-check'
def run(args,**kw):
    r=subprocess.run(args,cwd=ROOT,text=True,capture_output=True,timeout=60,**kw)
    if r.returncode:raise RuntimeError(f'{args}\n{r.stdout}\n{r.stderr}')
    return r

def main():
    env={**os.environ,'SDL_VIDEODRIVER':'unavailable','DISPLAY':'','WAYLAND_DISPLAY':''}
    for backend in ['SCRIPTED','SDL']:
        b=OUT/backend.lower()
        run(['cmake','-S',str(SOURCE),'-B',str(b),'-DSTUDY_PLATFORM='+backend,'-DCMAKE_BUILD_TYPE=Release','-DCMAKE_EXPORT_COMPILE_COMMANDS=ON','-DCMAKE_CXX_FLAGS=-Wall -Wextra -Wpedantic'])
        r=run(['cmake','--build',str(b),'-j3']);assert 'warning:' not in r.stderr,r.stderr
        print(run(['ctest','--test-dir',str(b),'--output-on-failure'],env=env).stdout.strip())
        for row in json.loads((b/'compile_commands.json').read_text()):
            if 'grid_demo' in row['file'] or 'grid_contract' in row['file']:
                assert 'SDL2' not in row['command'],row
        for name in ['grid_demo','grid_contract']:
            link=(b/'CMakeFiles'/f'{name}.dir/link.txt').read_text()
            assert all(x not in link for x in ['SDL','study_gl','study_platform']),link
        out=run([str(b/'grid_demo')],env=env).stdout
        assert '(1,2) -> index 12 -> (1,2)' in out
        assert 'get(0,0): present=1; get(0,1): present=1; get(0,10): present=0' in out
        assert 'original empty=0, copy empty=1' in out
        rows=[line for line in out.splitlines() if len(line)>=3 and line[:2].isdigit()]
        expected=['#.........','..#.......']+['..........']*17+['.........#']
        assert [line[3:] for line in rows]==expected,rows
        print(out.strip())
    flags=['-std=c++17','-DNDEBUG','-fsanitize=undefined','-fno-sanitize-recover=all']
    for source,inc,name in [(SOURCE/'tests/grid_contract.cpp',SOURCE,'sanitized_grid'),
                            (ROOT/'tests/learning/current_grid.cpp',ROOT,'current_grid')]:
        run(['c++',*flags,'-I'+str(inc),str(source),'-o',str(OUT/name)])
        print(run([str(OUT/name)],env=env).stdout.strip())
    # A bad flat-only bound accepts (0,10). Compile-time guards are removed in
    # this negative control so the runtime test must still reject the mutant.
    m=OUT/'mutation';(m/'simulation').mkdir(parents=True,exist_ok=True)
    s=(SOURCE/'simulation/grid.h').read_text()
    needle='return row >= 0 && row < kRows && column >= 0 && column < kColumns;';assert needle in s
    s=s.replace(needle,'return row >= 0 && row < kRows && column >= 0 && row * kColumns + column < static_cast<int>(kCount);')
    (m/'simulation/grid.h').write_text(s)
    test=(SOURCE/'tests/grid_contract.cpp').read_text().replace('static_assert(!Grid::index_of(0,10));','')
    (m/'check.cpp').write_text(test)
    run(['c++','-std=c++17','-DNDEBUG','-I'+str(m),str(m/'check.cpp'),'-o',str(OUT/'flat_only')])
    r=subprocess.run([str(OUT/'flat_only')],text=True,capture_output=True,timeout=5);assert r.returncode==1 and 'grid line' in r.stderr
    print('Flat-only boundary negative control rejected in Release; no GL/display dependency in board executables.')
if __name__=='__main__':main()
