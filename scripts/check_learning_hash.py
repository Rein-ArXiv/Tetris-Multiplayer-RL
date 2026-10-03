"""Canonical teaching bytes and compatibility-preserving production hash checks."""
from pathlib import Path
import os,subprocess,struct
ROOT=Path(__file__).resolve().parents[1]
SOURCE=ROOT/'docs/learn/checkpoints/52-state-hash'
OUT=ROOT/'out/learning-checkpoints/52-state-hash-check'
def run(args,**kw):
    r=subprocess.run(args,cwd=ROOT,text=True,capture_output=True,timeout=kw.pop('timeout',360),**kw)
    if r.returncode:raise RuntimeError(f'{args}\n{r.stdout}\n{r.stderr}')
    return r

def main():
    OUT.mkdir(parents=True,exist_ok=True)
    changed={'simulation/piece_source.h','simulation/scripted_source.h','src/main.cpp','CMakeLists.txt','README.md'}
    for p in (ROOT/'docs/learn/checkpoints/51-rng-streams').rglob('*'):
        rel=p.relative_to(ROOT/'docs/learn/checkpoints/51-rng-streams')
        if p.is_file() and str(rel) not in changed:assert p.read_bytes()==(SOURCE/rel).read_bytes()
    env={**os.environ,'SDL_VIDEODRIVER':'dummy','DISPLAY':'','WAYLAND_DISPLAY':''}
    for backend in ['SCRIPTED','SDL']:
        build=OUT/backend.lower()
        run(['cmake','-S',str(SOURCE),'-B',str(build),'-DSTUDY_PLATFORM='+backend,'-DCMAKE_BUILD_TYPE=Release','-DCMAKE_CXX_FLAGS=-Wall -Wextra -Wpedantic'])
        r=run(['cmake','--build',str(build),'-j3']);(OUT/f'build-{backend}.log').write_text(r.stdout+r.stderr);assert 'warning:' not in r.stdout+r.stderr
        r=run(['ctest','--test-dir',str(build),'--output-on-failure'],env=env);(OUT/f'ctest-{backend}.log').write_text(r.stdout);print(r.stdout.strip(),flush=True)
        output=run([str(build/'hash_demo')]).stdout;(OUT/f'demo-{backend}.log').write_text(output)
        rows=output.splitlines();assert rows[:3]==[
            'LRND/1 tick=0 bytes=342 row=0 gravity=0 hash=c799419b70bbe2de',
            'LRND/1 tick=1 bytes=342 row=0 gravity=1 hash=c3409b53b6d47de1',
            'LRND/1 tick=2 bytes=342 row=0 gravity=2 hash=32a79d4c3c185c58']
        actual=bytes.fromhex(rows[3].split('=')[1]);expected=b'LRND'+struct.pack('<III',1,20,10)+bytes(200)
        expected+=struct.pack('<BBiB',0,6,0,1)+struct.pack('<10i',0,3,0,1,1,0,1,1,1,2)
        expected+=bytes([3,7,4,2,1])+struct.pack('<Q',2306758490171379329)+bytes([3,3,1,5,1])+struct.pack('<Q',1^0x9E3779B97F4A7C15)
        expected+=struct.pack('<4iB4Qi',2,30,0,4,0,0,0,0,0,0)
        assert actual==expected
        h=14695981039346656037
        for b in expected:h=((h^b)*1099511628211)&((1<<64)-1)
        assert h==0x32a79d4c3c185c58 and rows[4]=='8-bit collision: 143 and 256 -> 4a'
        assert 'SDL' not in (build/'CMakeFiles/hash_contract.dir/link.txt').read_text()
    flags=['c++','-std=c++17','-O1','-DNDEBUG','-fsanitize=undefined','-fno-sanitize-recover=all']
    run([*flags,'-I'+str(SOURCE),str(SOURCE/'tests/hash_contract.cpp'),'-o',str(OUT/'sanitized')]);print(run([str(OUT/'sanitized')]).stdout.strip(),flush=True)
    text=(ROOT/'src/sim_game.cpp').read_text();body=text[text.index('SimGame::HashBreakdown SimGame::StateHashBreakdown()'):text.index('// ============================================================================',text.index('uint64_t SimGame::StateHash()'))].replace('SimGame::','HashProbe::')
    (OUT/'production_hash.inc').write_text(body)
    run([*flags,'-I'+str(ROOT),'-I'+str(OUT),str(ROOT/'tests/learning/current_hash.cpp'),str(ROOT/'src/position.cpp'),'-o',str(OUT/'production')]);print(run([str(OUT/'production')]).stdout.strip(),flush=True)
    for name,path,before,after in [
        ('omit_gravity','simulation/state_hash.h','out.i32(round.gravity().elapsed);','out.i32(0);'),
        ('omit_bag','simulation/state_hash.h','i < seeded.bag().remaining()','i < 0'),
        ('truncate_success','core/canonical_bytes.h','if (!ok_) return std::nullopt;','// BUG: allow truncated success')]:
        root=OUT/name;p=root/path;p.parent.mkdir(parents=True,exist_ok=True);text=(SOURCE/path).read_text();assert before in text;p.write_text(text.replace(before,after))
        run(['c++','-std=c++17','-O2','-DNDEBUG','-I'+str(root),'-I'+str(SOURCE),str(SOURCE/'tests/hash_contract.cpp'),'-o',str(root/'check')])
        r=subprocess.run([str(root/'check')],text=True,capture_output=True,timeout=30)
        assert r.returncode==1 and 'CHECK failed' in r.stderr,name;(root/'result.log').write_text(r.stderr)
    build=OUT/'root'
    run(['cmake','-S',str(ROOT),'-B',str(build),'-DCMAKE_BUILD_TYPE=Release','-DTETRIS_BUILD_REACTOR=OFF'])
    r=run(['cmake','--build',str(build),'-j3'],timeout=600);(OUT/'root-build.log').write_text(r.stdout+r.stderr)
    r=run(['ctest','--test-dir',str(build),'--output-on-failure'],env=env);(OUT/'root-ctest.log').write_text(r.stdout);print(r.stdout.strip(),flush=True)
    output=run([str(build/'sim_hash_dump')]).stdout
    assert output==(ROOT/'python/tests/_sim_hash_dump.txt').read_text()
    (OUT/'legacy-dump.log').write_text(output)
    for kind,value in [('bool','true'),('pointer','static_cast<int*>(nullptr)'),('object','Sample{}')]:
        p=OUT/f'reject-{kind}.cpp';p.write_text('#include "core/hash.h"\nstruct Sample { int value; };\nint main(){return int(fnv1a64_value('+value+'));}\n')
        r=subprocess.run(['c++','-std=c++17','-I'+str(ROOT),'-c',str(p),'-o',str(OUT/f'{kind}.o')],text=True,capture_output=True)
        assert r.returncode!=0 and 'hash a fixed-width integer' in r.stderr
        (OUT/f'reject-{kind}.log').write_text(r.stderr)
    print('Canonical bytes independently reconstructed, 60k teaching ticks, production omission probe, UBSan, three Release mutations, root game/tests and unchanged legacy golden passed.',flush=True)
if __name__=='__main__':main()
