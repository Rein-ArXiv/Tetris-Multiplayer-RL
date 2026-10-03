"""Verify the CPU presentation adapter and real Game ownership boundary."""
from pathlib import Path
import os,subprocess
ROOT=Path(__file__).resolve().parents[1]
SOURCE=ROOT/'docs/learn/checkpoints/54-game-adapter'
OUT=ROOT/'out/learning-checkpoints/54-game-adapter-check'
def run(args,**kw):
    r=subprocess.run(args,cwd=ROOT,text=True,capture_output=True,timeout=kw.pop('timeout',360),**kw)
    if r.returncode:raise RuntimeError(f'{args}\n{r.stdout}\n{r.stderr}')
    return r

def main():
    OUT.mkdir(parents=True,exist_ok=True)
    previous=ROOT/'docs/learn/checkpoints/53-golden-regression'
    for p in previous.rglob('*'):
        rel=p.relative_to(previous)
        if p.is_file() and str(rel)not in {'CMakeLists.txt','README.md','src/main.cpp'}:assert p.read_bytes()==(SOURCE/rel).read_bytes()
    env={**os.environ,'SDL_VIDEODRIVER':'dummy','SDL_AUDIODRIVER':'dummy','DISPLAY':'','WAYLAND_DISPLAY':''}
    for backend in ['SCRIPTED','SDL']:
        build=OUT/backend.lower()
        run(['cmake','-S',str(SOURCE),'-B',str(build),'-DSTUDY_PLATFORM='+backend,'-DCMAKE_BUILD_TYPE=Release','-DCMAKE_CXX_FLAGS=-Wall -Wextra -Wpedantic'])
        r=run(['cmake','--build',str(build),'-j3']);(OUT/f'build-{backend}.log').write_text(r.stdout+r.stderr);assert 'warning:' not in r.stdout+r.stderr
        r=run(['ctest','--test-dir',str(build),'--output-on-failure'],env=env);(OUT/f'ctest-{backend}.log').write_text(r.stdout);print(r.stdout.strip(),flush=True)
        output=run([str(build/'adapter_demo')]).stdout;(OUT/f'demo-{backend}.log').write_text(output)
        assert 'ticks=1' in output and 'after first next=4' in output
        link=(build/'CMakeFiles/adapter_contract.dir/link.txt').read_text();assert 'SDL' not in link and 'study_platform' not in link
    flags=['c++','-std=c++17','-O1','-DNDEBUG','-fsanitize=undefined','-fno-sanitize-recover=all']
    run([*flags,'-I'+str(SOURCE),str(SOURCE/'tests/adapter_contract.cpp'),'-o',str(OUT/'adapter-sanitized')]);print(run([str(OUT/'adapter-sanitized')]).stdout.strip(),flush=True)
    sources=[str(ROOT/p)for p in ['tests/game_wrapper_test.cpp','src/game.cpp','src/sim_game.cpp','src/position.cpp','src/colors.cpp']]
    run([*flags,'-I'+str(ROOT),*sources,'-o',str(OUT/'root-sanitized')]);print(run([str(OUT/'root-sanitized')]).stdout.strip(),flush=True)
    for name,before,after in [
        ('double_advance','auto report = runner_.advance(seconds, input);','auto report = runner_.advance(seconds, input);\n        (void)runner_.advance(seconds,input);'),
        ('skip_first_lock','for (unsigned i = 0; i < report->ticks; ++i)','for (unsigned i = 1; i < report->ticks; ++i)'),
        ('consume_on_read','return make_view(runner_.round());','(void)const_cast<Game*>(this)->accent_.sample(); return make_view(runner_.round());')]:
        folder=OUT/name;p=folder/'client/game.h';p.parent.mkdir(parents=True,exist_ok=True);text=(SOURCE/'client/game.h').read_text();assert before in text;p.write_text(text.replace(before,after))
        run(['c++','-std=c++17','-O2','-DNDEBUG','-I'+str(folder),'-I'+str(SOURCE),str(SOURCE/'tests/adapter_contract.cpp'),'-o',str(folder/'check')])
        r=subprocess.run([str(folder/'check')],capture_output=True,text=True);assert r.returncode==1 and 'CHECK failed' in r.stderr,name;(folder/'result.log').write_text(r.stderr)
    for name,body in [('copy','Game other(original);'),('move','Game other(std::move(original));')]:
        p=OUT/f'reject-{name}.cpp';p.write_text('#include "src/game.h"\n#include <utility>\nvoid f(Game& original){'+body+'}\n')
        r=subprocess.run(['c++','-std=c++17','-I'+str(ROOT),'-c',str(p),'-o',str(OUT/f'{name}.o')],capture_output=True,text=True)
        assert r.returncode!=0 and 'deleted' in r.stderr;(OUT/f'reject-{name}.log').write_text(r.stderr)
    build=OUT/'root'
    run(['cmake','-S',str(ROOT),'-B',str(build),'-DTETRIS_BUILD_REACTOR=OFF','-DCMAKE_BUILD_TYPE=Release'])
    r=run(['cmake','--build',str(build),'-j3'],timeout=600);(OUT/'root-build.log').write_text(r.stdout+r.stderr)
    r=run(['ctest','--test-dir',str(build),'--output-on-failure'],env=env);(OUT/'root-ctest.log').write_text(r.stdout);print(r.stdout.strip(),flush=True)
    assert run([str(build/'sim_hash_dump')]).stdout==(ROOT/'python/tests/_sim_hash_dump.txt').read_text()
    print('Adapter/full-game builds, unchanged goldens, snapshots, event folding, actual Game probes, forbidden copies, UBSan and three Release mutations passed.',flush=True)
if __name__=='__main__':main()
