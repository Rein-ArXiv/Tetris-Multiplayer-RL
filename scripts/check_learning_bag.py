"""Check seven-bag state, all selection paths, boundaries and production parity."""
from pathlib import Path
import os
import subprocess
ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / 'docs/learn/checkpoints/49-seven-bag'
OUT = ROOT / 'out/learning-checkpoints/49-seven-bag-check'

def run(args, **kwargs):
    result = subprocess.run(args, cwd=kwargs.pop('cwd', ROOT), text=True,
                            capture_output=True, timeout=kwargs.pop('timeout', 240), **kwargs)
    if result.returncode:
        raise RuntimeError(f'{args}\n{result.stdout}\n{result.stderr}')
    return result

def production_parity():
    includes = ['-I'+str(ROOT), '-I'+str(SOURCE)]
    common = ['c++', '-std=c++17', '-O1', '-DNDEBUG', '-Wall', '-Wextra', '-Wpedantic',
              '-fsanitize=undefined', '-fno-sanitize-recover=all', *includes]
    result = run([*common, str(ROOT/'tests/learning/current_bag.cpp'), str(ROOT/'src/sim_game.cpp'),
                  str(ROOT/'src/position.cpp'), '-o', str(OUT/'current-bag')])
    assert 'warning:' not in result.stderr
    print(run([str(OUT/'current-bag')]).stdout.strip(), flush=True)
    # Extract the production draw/factory verbatim, changing only the class qualifier.
    # This isolates long supply sequences from finite game-over, without private macros.
    source = (ROOT/'src/sim_game.cpp').read_text()
    functions = source[source.index('SimBlock SimGame::GetRandomBlock()'):source.index('SimBlock SimGame::MakeGhostBlock(')]
    functions = functions.replace('SimGame::', 'ProductionBagProbe::')
    prefix = '''#include "src/sim_blocks.h"
#include "core/rng.h"
#include "simulation/seven_bag.h"
#include <cstdio>
#include <cstdlib>
#define CHECK(x) do { if (!(x)) { std::fprintf(stderr, "CHECK failed at %d: %s\\n", __LINE__, #x); std::exit(1); } } while (false)
class ProductionBagProbe {
public:
    explicit ProductionBagProbe(std::uint64_t seed) : rng(seed ? seed : 0xC0FFEE123456789ull) { blocks = GetAllBlocks(); }
    SimBlock GetRandomBlock();
    std::vector<SimBlock> GetAllBlocks() const;
    std::vector<SimBlock> blocks;
    XorShift64Star rng;
};
'''
    suffix = '''
int main() {
    for (std::uint64_t seed = 0; seed < 100; ++seed) {
        ProductionBagProbe production(seed);
        XorShift64Star rng(seed ? seed : 0xC0FFEE123456789ull);
        study_bag::SevenBag bag;
        for (int draw = 0; draw < 1000; ++draw) {
            const auto selected = bag.take(rng.nextUInt(static_cast<std::uint32_t>(bag.next_bound())));
            CHECK(selected);
            CHECK(production.GetRandomBlock().id == static_cast<int>(*selected));
            CHECK(production.rng.getState() == rng.getState());
            CHECK(production.blocks.size() == bag.remaining());
            for (std::size_t i = 0; i < bag.remaining(); ++i) CHECK(production.blocks[i].id == static_cast<int>(*bag.at(i)));
        }
    }
    std::puts("100000 extracted production draws match stable prefix, refills, output IDs and RNG state including one-item draws");
}
'''
    fixture = OUT/'production-draw.cpp'
    fixture.write_text(prefix+functions+suffix)
    result = run([*common, str(fixture), str(ROOT/'src/position.cpp'), '-o', str(OUT/'production-draw')])
    assert 'warning:' not in result.stderr
    print(run([str(OUT/'production-draw')]).stdout.strip(), flush=True)

def main():
    OUT.mkdir(parents=True, exist_ok=True)
    old = ROOT/'docs/learn/checkpoints/48-input-mask'
    for path in old.rglob('*'):
        if path.is_file() and str(path.relative_to(old)) not in {'README.md', 'CMakeLists.txt'}:
            assert path.read_bytes() == (SOURCE/path.relative_to(old)).read_bytes()
    env = {**os.environ, 'SDL_VIDEODRIVER':'dummy', 'DISPLAY':'', 'WAYLAND_DISPLAY':''}
    expected = '''first-only x9: I J L O S T Z I J
last-only x9: Z T S O L J I Z T
empty: remaining=0 next_bound=7
take(7) at empty -> null, remaining=0
round current=O preview=IZL cursor=4
after 3 locks current=L preview=TJS cursor=0
boundary: I J L O S T Z | I J L
toy counts=3/3/2 (toy, not actual RNG)
'''
    for backend in ['SCRIPTED','SDL']:
        build = OUT/backend.lower()
        run(['cmake','-S',str(SOURCE),'-B',str(build),'-DSTUDY_PLATFORM='+backend,
             '-DCMAKE_BUILD_TYPE=Release','-DCMAKE_CXX_FLAGS=-Wall -Wextra -Wpedantic'])
        targets = [] if backend == 'SCRIPTED' else ['--target','tetris','bag_demo','bag_contract','queue_contract','frame_contract']
        result = run(['cmake','--build',str(build),'-j3',*targets],timeout=360)
        (OUT/f'build-{backend}.log').write_text(result.stdout+result.stderr)
        assert 'warning:' not in result.stdout+result.stderr
        pattern = [] if backend == 'SCRIPTED' else ['-R','^(bag_contract|queue_contract|frame_contract)$']
        result = run(['ctest','--test-dir',str(build),'--output-on-failure',*pattern],env=env)
        (OUT/f'ctest-{backend}.log').write_text(result.stdout)
        print(result.stdout.strip(),flush=True)
        text = run([str(build/'bag_demo')],env=env).stdout
        (OUT/f'bag_demo-{backend}.log').write_text(text)
        assert text == expected
        link = (build/'CMakeFiles/bag_demo.dir/link.txt').read_text()
        assert 'SDL' not in link and 'study_platform' not in link
    run(['c++','-std=c++17','-O1','-DNDEBUG','-fsanitize=undefined','-fno-sanitize-recover=all',
         '-I'+str(SOURCE),str(SOURCE/'tests/bag_contract.cpp'),'-o',str(OUT/'bag-sanitized')])
    print(run([str(OUT/'bag-sanitized')]).stdout.strip(),flush=True)
    production_parity()
    header = (SOURCE/'simulation/seven_bag.h').read_text()
    erase = '''        for (std::size_t i = index; i + 1 < count_; ++i) {
            slots_[i] = slots_[i + 1];
        }'''
    for name, old_text, new_text in [
        ('swap_tail', erase, '        slots_[index] = slots_[count_ - 1];'),
        ('refill_before_check', '        if (index >= capacity || (count_ != 0 && index >= count_)) {', '        if (count_ == 0) refill();\n        if (index >= capacity || (count_ != 0 && index >= count_)) {'),
        ('no_erase', erase, '        // mutant: selected item remains in active prefix')]:
        path = OUT/name/'simulation/seven_bag.h';path.parent.mkdir(parents=True,exist_ok=True)
        assert old_text in header
        path.write_text(header.replace(old_text,new_text))
        binary = OUT/name/'check'
        run(['c++','-std=c++17','-DNDEBUG','-I'+str(OUT/name),'-I'+str(SOURCE),
             str(SOURCE/'tests/bag_contract.cpp'),'-o',str(binary)])
        result = subprocess.run([str(binary)],capture_output=True,text=True,timeout=30)
        assert result.returncode == 1 and 'CHECK failed' in result.stderr, name
        (OUT/name/'result.log').write_text(result.stderr)
    print('SevenBag: both builds warning-free; CPU-only linkage; all permutations and Round integration; production parity; UBSan; three Release mutations rejected.',flush=True)
if __name__ == '__main__':
    main()
