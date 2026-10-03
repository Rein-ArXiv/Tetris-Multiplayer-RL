"""Check screen transitions against real tutorial rounds and restart boundaries."""
from pathlib import Path
import os, subprocess
ROOT=Path(__file__).resolve().parents[1]
SOURCE=ROOT/'docs/learn/checkpoints/55-screen-state'
OUT=ROOT/'out/learning-checkpoints/55-screen-state-check'
def run(args,**kw):
    r=subprocess.run(args,cwd=ROOT,text=True,capture_output=True,timeout=kw.pop('timeout',600),**kw)
    if r.returncode: raise RuntimeError(f'{args}\n{r.stdout}\n{r.stderr}')
    return r

def main():
    OUT.mkdir(parents=True,exist_ok=True)
    previous=SOURCE.parent/'54-game-adapter'
    for p in previous.rglob('*'):
        rel=p.relative_to(previous)
        if p.is_file() and str(rel) not in {'CMakeLists.txt','README.md','src/main.cpp'}:
            assert p.read_bytes()==(SOURCE/rel).read_bytes(),rel
    env={**os.environ,'SDL_VIDEODRIVER':'dummy','SDL_AUDIODRIVER':'dummy','DISPLAY':'','WAYLAND_DISPLAY':''}
    for backend in ['SCRIPTED','SDL']:
        build=OUT/backend.lower()
        run(['cmake','-S',str(SOURCE),'-B',str(build),'-DSTUDY_PLATFORM='+backend,'-DCMAKE_BUILD_TYPE=Release','-DCMAKE_CXX_FLAGS=-Wall -Wextra -Wpedantic'])
        r=run(['cmake','--build',str(build),'-j3']);(OUT/f'build-{backend}.log').write_text(r.stdout+r.stderr);assert 'warning:' not in r.stdout+r.stderr
        r=run(['ctest','--test-dir',str(build),'--output-on-failure'],env=env);(OUT/f'ctest-{backend}.log').write_text(r.stdout);print(r.stdout.strip(),flush=True)
        out=run([str(build/'screen_demo')]).stdout;(OUT/f'demo-{backend}.log').write_text(out)
        assert 'start owns game=1 phase=0 frame=0' in out and 'playing ticks=1 locked=1' in out and 'restart baseline restored=1 phase=0' in out
        link=(build/'CMakeFiles/screen_contract.dir/link.txt').read_text();assert 'SDL' not in link and 'study_platform' not in link
    flags=['c++','-std=c++17','-O1','-DNDEBUG','-fsanitize=undefined','-fno-sanitize-recover=all']
    run([*flags,'-I'+str(SOURCE),str(SOURCE/'tests/screen_contract.cpp'),'-o',str(OUT/'screen-sanitized')]);print(run([str(OUT/'screen-sanitized')]).stdout.strip(),flush=True)
    original=(SOURCE/'client/application.h').read_text()
    for name,before,after in [
      ('retain_old_game','game_.emplace(initial_);','if (!game_) game_.emplace(initial_);'),
      ('leak_start_drop','return report; // The confirm edge is not also a hard drop.','armed_ = true; // Deliberate fallthrough defect.'),
      ('retain_pending_on_cancel','input.cancelled = cancelled;','input.cancelled = false;')]:
        folder=OUT/name;p=folder/'client/application.h';p.parent.mkdir(parents=True,exist_ok=True)
        assert before in original;p.write_text(original.replace(before,after))
        run(['c++','-std=c++17','-O2','-DNDEBUG','-I'+str(folder),'-I'+str(SOURCE),str(SOURCE/'tests/screen_contract.cpp'),'-o',str(folder/'check')])
        r=subprocess.run([str(folder/'check')],capture_output=True,text=True);assert r.returncode==1 and 'CHECK failed' in r.stderr,name;(folder/'result.log').write_text(r.stderr)
    # Execute the actual lambda body with resource-free collaborators. This proves
    # caller-state reset and failure order; real Game ownership is tested separately.
    text=(ROOT/'src/main.cpp').read_text()
    start=text.index('    auto beginSingleRound = [&]() {');end=text.index('\n    };',start)+len('\n    };')
    body=text[start:end]
    probe='''#include <memory>
#include <vector>
#include <stdexcept>
#include <cstdint>
#include <cstdlib>
#define CHECK(x) do{if(!(x))std::abort();}while(false)
struct Game { static bool fail; std::uint64_t seed; explicit Game(std::uint64_t s):seed(s){if(fail)throw std::runtime_error("init");} }; bool Game::fail=false;
enum class AppMode {Menu,Single};
int main(){
std::uint64_t sessionSeed=42;std::unique_ptr<Game> gameSingle=std::make_unique<Game>(1);
float accumulator=.015f;std::uint8_t s_pendingInput=31;int s_leftHoldTicks=9,s_rightHoldTicks=7;
struct Fx{int value=0;};Fx coLocal{5},shakeLeft{6};
bool recording=true;struct Replay{std::vector<int> frames;std::uint64_t seed;};Replay replay{{1,2},1};
AppMode app=AppMode::Menu;
'''+body+'''
Game::fail=true;try{beginSingleRound();CHECK(false);}catch(const std::runtime_error&){}
CHECK(gameSingle->seed==1&&accumulator==.015f&&s_pendingInput==31&&replay.frames.size()==2&&app==AppMode::Menu);
Game::fail=false;beginSingleRound();
CHECK(gameSingle->seed==42&&accumulator==0&&s_pendingInput==0&&s_leftHoldTicks==0&&s_rightHoldTicks==0);
CHECK(coLocal.value==0&&shakeLeft.value==0&&replay.frames.empty()&&replay.seed==42&&app==AppMode::Single);
recording=false;replay.frames={7};beginSingleRound();CHECK(replay.frames.size()==1);
}
'''
    p=OUT/'root-reset-probe.cpp';p.write_text(probe)
    run([*flags,str(p),'-o',str(OUT/'root-reset-probe')]);run([str(OUT/'root-reset-probe')])
    assert text.count('beginSingleRound();')==2
    # Reuse the prior root build; rebuild actual changed main, then regression tests.
    build=ROOT/'out/learning-checkpoints/54-game-adapter-check/root'
    run(['cmake','-S',str(ROOT),'-B',str(build),'-DTETRIS_BUILD_REACTOR=OFF','-DCMAKE_BUILD_TYPE=Release'])
    r=run(['cmake','--build',str(build),'-j3']);(OUT/'root-build.log').write_text(r.stdout+r.stderr)
    r=run(['ctest','--test-dir',str(build),'--output-on-failure'],env=env);(OUT/'root-ctest.log').write_text(r.stdout);print(r.stdout.strip(),flush=True)
    assert run([str(build/'sim_hash_dump')]).stdout==(ROOT/'python/tests/_sim_hash_dump.txt').read_text()
    print('Screen/full-game builds, transition table, restart isolation, input gate, terminal freeze, root reset probe, UBSan and three Release mutations passed.',flush=True)
if __name__=='__main__':main()
