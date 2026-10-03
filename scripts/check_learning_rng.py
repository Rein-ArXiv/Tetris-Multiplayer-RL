"""Validate the owned seeded supply through engine, Round, frames and actual CLI."""
from pathlib import Path
import os, subprocess
ROOT=Path(__file__).resolve().parents[1]
SOURCE=ROOT/'docs/learn/checkpoints/50-seeded-rng'
OUT=ROOT/'out/learning-checkpoints/50-seeded-rng-check'
def run(args, **kw):
    result=subprocess.run(args,cwd=kw.pop('cwd',ROOT),text=True,capture_output=True,timeout=kw.pop('timeout',240),**kw)
    if result.returncode:raise RuntimeError(f'{args}\n{result.stdout}\n{result.stderr}')
    return result

def main():
    OUT.mkdir(parents=True,exist_ok=True)
    old=ROOT/'docs/learn/checkpoints/49-seven-bag'
    for p in old.rglob('*'):
        if p.is_file() and str(p.relative_to(old)) not in {'README.md','CMakeLists.txt','simulation/round.h','src/main.cpp'}:
            assert p.read_bytes()==(SOURCE/p.relative_to(old)).read_bytes()
    assert (ROOT/'core/rng.h').read_bytes()==(SOURCE/'core/rng.h').read_bytes()
    env={**os.environ,'SDL_VIDEODRIVER':'dummy','DISPLAY':'','WAYLAND_DISPLAY':''}
    for backend in ['SCRIPTED','SDL']:
        build=OUT/backend.lower()
        run(['cmake','-S',str(SOURCE),'-B',str(build),'-DSTUDY_PLATFORM='+backend,'-DCMAKE_BUILD_TYPE=Release','-DCMAKE_CXX_FLAGS=-Wall -Wextra -Wpedantic'])
        # Both full suites: Round storage changed, so all rule consumers need coverage.
        result=run(['cmake','--build',str(build),'-j3'],timeout=360)
        (OUT/f'build-{backend}.log').write_text(result.stdout+result.stderr);assert 'warning:' not in result.stdout+result.stderr
        result=run(['ctest','--test-dir',str(build),'--output-on-failure'],env=env)
        (OUT/f'ctest-{backend}.log').write_text(result.stdout);print(result.stdout.strip(),flush=True)
        for seed in ['1','0','42','18446744073709551615']:
            text=run([str(build/'rng_demo'),seed],env=env).stdout
            (OUT/f'demo-{backend}-{seed}.log').write_text(text)
            if seed=='1':
                for row in ['engine initial=1','next state=33554433 output=5180492295206395165','pieces=TZOJSLI|LOITSJZ','locks=0 current=T preview=ZOJ remaining=3','locks=3 current=J preview=SLI remaining=0']:assert row in text,row
            if seed=='0':assert 'engine initial=88172645463393265' in text and 'bag initial=869193496018642825' in text
        for text in ['-1','+1','18446744073709551616','1e3','']:
            assert subprocess.run([str(build/'rng_demo'),text],capture_output=True).returncode==2
        link=(build/'CMakeFiles/rng_demo.dir/link.txt').read_text();assert 'SDL' not in link and 'study_platform' not in link
        if backend=='SDL':
            for args in [['--help'],['--seed'],['--seed','-1'],['--seed','18446744073709551616'],['--seed','42','normal']]:
                r=subprocess.run([str(build/'tetris'),*args],env=env,capture_output=True,text=True,timeout=10)
                assert r.returncode==(0 if args==['--help'] else 2)
            for seed in ['0','1','42','18446744073709551615']:
                r=subprocess.run([str(build/'tetris'),'--seed',seed],env=env,capture_output=True,text=True,timeout=10)
                # dummy cannot create a desktop GL window; verify parsed setup before that failure.
                assert r.returncode==1 and 'Seeded 7-bag: requested='+seed in r.stdout and 'current=' in r.stdout
                if seed=='1':assert 'current=T preview=ZOJ' in r.stdout
                (OUT/f'cli-{seed}.log').write_text(r.stdout+r.stderr)
    flags=['c++','-std=c++17','-O1','-DNDEBUG','-fsanitize=undefined','-fno-sanitize-recover=all']
    run([*flags,'-I'+str(SOURCE),str(SOURCE/'tests/rng_contract.cpp'),'-o',str(OUT/'rng-sanitized')])
    print(run([str(OUT/'rng-sanitized')]).stdout.strip(),flush=True)
    text=(ROOT/'src/sim_game.cpp').read_text();functions=text[text.index('SimBlock SimGame::GetRandomBlock()'):text.index('SimBlock SimGame::MakeGhostBlock(')].replace('SimGame::','ProductionBagProbe::')
    (OUT/'production_bag_functions.inc').write_text(functions)
    run([*flags,'-I'+str(ROOT),'-I'+str(SOURCE),'-I'+str(OUT),str(ROOT/'tests/learning/current_seeded_source.cpp'),str(ROOT/'src/sim_game.cpp'),str(ROOT/'src/position.cpp'),'-o',str(OUT/'production')])
    print(run([str(OUT/'production')]).stdout.strip(),flush=True)
    for name,path,old_text,new_text in [
        ('output_as_state','core/rng.h','state = x;','state = x * 2685821657736338717ull;'),
        ('skip_one','simulation/seeded_bag_source.h','rng_.nextUInt(bound)','(bound == 1 ? 0 : rng_.nextUInt(bound))'),
        ('wrong_zero','simulation/seeded_bag_source.h','rng_(seed ? seed : default_seed)','rng_(seed)')]:
        root=OUT/name;p=root/path;p.parent.mkdir(parents=True,exist_ok=True)
        text=(SOURCE/path).read_text();assert old_text in text;p.write_text(text.replace(old_text,new_text))
        run(['c++','-std=c++17','-O2','-DNDEBUG','-I'+str(root),'-I'+str(SOURCE),str(SOURCE/'tests/rng_contract.cpp'),'-o',str(root/'check')])
        r=subprocess.run([str(root/'check')],capture_output=True,text=True,timeout=30)
        assert r.returncode==1 and 'CHECK failed' in r.stderr,name
        (root/'result.log').write_text(r.stderr)
    print('Owned RNG: full SCRIPTED/SDL CTests, warning-free builds, CPU output/linkage, real CLI setup under dummy, UBSan, production parity and three Release mutations passed.',flush=True)
if __name__=='__main__':main()
