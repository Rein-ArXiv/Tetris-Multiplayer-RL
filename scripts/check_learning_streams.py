"""Validate owned rule/presentation streams and their actual game connections."""
from pathlib import Path
import os,subprocess
ROOT=Path(__file__).resolve().parents[1]
SOURCE=ROOT/'docs/learn/checkpoints/51-rng-streams'
OUT=ROOT/'out/learning-checkpoints/51-rng-streams-check'
def run(args,**kw):
    r=subprocess.run(args,cwd=kw.pop('cwd',ROOT),text=True,capture_output=True,timeout=kw.pop('timeout',240),**kw)
    if r.returncode:raise RuntimeError(f'{args}\n{r.stdout}\n{r.stderr}')
    return r

def main():
    OUT.mkdir(parents=True,exist_ok=True)
    old=ROOT/'docs/learn/checkpoints/50-seeded-rng'
    changed={'CMakeLists.txt','README.md','simulation/round.h','simulation/seeded_bag_source.h','renderer/board_scene.h','src/main.cpp'}
    for p in old.rglob('*'):
        if p.is_file() and str(p.relative_to(old)) not in changed:assert p.read_bytes()==(SOURCE/p.relative_to(old)).read_bytes()
    env={**os.environ,'SDL_VIDEODRIVER':'dummy','DISPLAY':'','WAYLAND_DISPLAY':''}
    for backend in ['SCRIPTED','SDL']:
        build=OUT/backend.lower()
        run(['cmake','-S',str(SOURCE),'-B',str(build),'-DSTUDY_PLATFORM='+backend,'-DCMAKE_BUILD_TYPE=Release','-DCMAKE_CXX_FLAGS=-Wall -Wextra -Wpedantic'])
        r=run(['cmake','--build',str(build),'-j3'],timeout=360);(OUT/f'build-{backend}.log').write_text(r.stdout+r.stderr);assert 'warning:' not in r.stdout+r.stderr
        r=run(['ctest','--test-dir',str(build),'--output-on-failure'],env=env);(OUT/f'ctest-{backend}.log').write_text(r.stdout);print(r.stdout.strip(),flush=True)
        output=run([str(build/'streams_demo')],env=env).stdout;(OUT/f'demo-{backend}.log').write_text(output)
        rows=output.splitlines();assert len(rows)==7
        for i,n in enumerate([0,1,37]):assert rows[i]==f'visual={n} current=Z hole=9 inserted=3 piece_state=585415316980522496 hole_state=285734347254379331'
        assert rows[3]=='xor-zero raw=0 engine=88172645463393265'
        assert rows[4:]==['shared visual=0 rule_index=5','shared visual=1 rule_index=1','shared visual=37 rule_index=2']
        link=(build/'CMakeFiles/streams_contract.dir/link.txt').read_text();assert 'SDL' not in link and 'study_platform' not in link
        if backend=='SDL':
            for args in [['--seed','1'],['--seed','1','garbage'],['T','garbage']]:
                r=subprocess.run([str(build/'tetris'),*args],env=env,capture_output=True,text=True,timeout=10)
                assert r.returncode==1
                if args[0]=='--seed':assert 'current=T preview=ZOJ' in r.stdout
                if len(args)==3:assert 'one seeded garbage hole' in r.stdout
                (OUT/('cli-'+'-'.join(args)+'.log')).write_text(r.stdout+r.stderr)
            for args in [['--seed','1','normal'],['T','garbage','garbage'],['--seed','-1','garbage'],['--seed','1','garbage','extra']]:
                assert subprocess.run([str(build/'tetris'),*args],env=env,capture_output=True,timeout=10).returncode==2
    flags=['c++','-std=c++17','-O1','-DNDEBUG','-fsanitize=undefined','-fno-sanitize-recover=all']
    run([*flags,'-I'+str(SOURCE),str(SOURCE/'tests/streams_contract.cpp'),str(SOURCE/'renderer/triangle.cpp'),'-o',str(OUT/'streams-sanitized')]);print(run([str(OUT/'streams-sanitized')]).stdout.strip(),flush=True)
    text=(ROOT/'src/sim_game.cpp').read_text();body=text[text.index('void SimGame::InsertGarbage(int rows)'):text.index('bool SimGame::BlockFits(')].replace('SimGame::','ProductionGarbageProbe::')
    (OUT/'production_garbage.inc').write_text(body)
    run([*flags,'-I'+str(ROOT),'-I'+str(SOURCE),'-I'+str(OUT),str(ROOT/'tests/learning/current_streams.cpp'),str(ROOT/'src/sim_game.cpp'),str(ROOT/'src/position.cpp'),'-o',str(OUT/'production')]);print(run([str(OUT/'production')]).stdout.strip(),flush=True)
    for name,path,before,after in [
        ('zero_after_xor','simulation/session_seed.h','return normalize(seed) ^ garbage_tag;','return normalize(seed ^ garbage_tag);'),
        ('consume_per_row','simulation/round.h','const int hole = static_cast<int>(candidate.holes_.next());','const int hole = static_cast<int>(candidate.holes_.next());\n            for(int extra=1;extra<candidate.pending_garbage_;++extra)(void)candidate.holes_.next();'),
        ('consume_zero','simulation/round.h','if (candidate.pending_garbage_ > 0) {','if (candidate.pending_garbage_ >= 0) {')]:
        root=OUT/name;p=root/path;p.parent.mkdir(parents=True,exist_ok=True);text=(SOURCE/path).read_text();assert before in text;p.write_text(text.replace(before,after))
        run(['c++','-std=c++17','-O2','-DNDEBUG','-I'+str(root),'-I'+str(SOURCE),str(SOURCE/'tests/streams_contract.cpp'),str(SOURCE/'renderer/triangle.cpp'),'-o',str(root/'check')])
        r=subprocess.run([str(root/'check')],text=True,capture_output=True,timeout=30);assert r.returncode==1 and 'CHECK failed' in r.stderr,name;(root/'result.log').write_text(r.stderr)
    print('Separated streams: full SCRIPTED/SDL suites, CPU/GL-call-recording contracts, real CLI setup, actual SimGame and extracted insertion parity, UBSan and three Release mutations passed.',flush=True)
if __name__=='__main__':main()
