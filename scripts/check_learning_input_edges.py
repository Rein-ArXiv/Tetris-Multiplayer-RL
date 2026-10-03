"""Check event/frame/tick input boundaries without a graphics context."""
from pathlib import Path
import os
import shlex
import subprocess
ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / 'docs/learn/checkpoints/47-input-edges'
OUT = ROOT / 'out/learning-checkpoints/47-input-edges-check'

def run(args, **kwargs):
    result = subprocess.run(args, cwd=ROOT, text=True, capture_output=True,
                            timeout=kwargs.pop('timeout', 240), **kwargs)
    if result.returncode:
        raise RuntimeError(f'{args}\n{result.stdout}\n{result.stderr}')
    return result

def main():
    OUT.mkdir(parents=True, exist_ok=True)
    assert (ROOT/'core/key_edges.h').read_bytes() == (SOURCE/'core/key_edges.h').read_bytes()
    env = {**os.environ, 'SDL_VIDEODRIVER': 'dummy', 'DISPLAY': '', 'WAYLAND_DISPLAY': ''}
    for backend in ['SCRIPTED', 'SDL']:
        build = OUT / backend.lower()
        run(['cmake', '-S', str(SOURCE), '-B', str(build), '-DSTUDY_PLATFORM=' + backend,
             '-DCMAKE_BUILD_TYPE=Release', '-DCMAKE_CXX_FLAGS=-Wall -Wextra -Wpedantic'])
        result = run(['cmake', '--build', str(build), '-j3'], timeout=360)
        (OUT/f'build-{backend}.log').write_text(result.stdout+result.stderr)
        assert 'warning:' not in result.stdout+result.stderr
        result=run(['ctest','--test-dir',str(build),'--output-on-failure'],env=env)
        (OUT/f'ctest-{backend}.log').write_text(result.stdout)
        print(result.stdout.strip(),flush=True)
    flags=shlex.split(run(['pkg-config','--cflags','--libs','sdl2']).stdout)
    binary=OUT/'root-platform'
    run(['c++','-std=c++17','-Wall','-Wextra','-Wpedantic',str(ROOT/'tests/learning/current_platform_focus.cpp'),*flags,'-o',str(binary)])
    print(run([str(binary)],env=env).stdout.strip(),flush=True)
    # Test the actual root functions, extracting a contiguous block, not a copy.
    main=(ROOT/'src/main.cpp').read_text()
    block=main[main.index('static uint8_t s_pendingInput'):main.index('// `host:port`')]
    harness=OUT/'root-input.cpp'
    harness.write_text('''#include "core/key_edges.h"
#include "core/input.h"
#include "platform/platform.h"
#include <cstdio>
#include <cstdlib>
static input_detail::KeyEdges<256> keys;
bool platform_key_pressed(int key){return keys.pressed(key);}
bool platform_key_down(int key){return keys.down(key);}
bool platform_input_cancelled(){return keys.cancelled();}
#define CHECK(x) do {if(!(x)){std::fprintf(stderr,"CHECK failed: %s\\n",#x);std::exit(1);}}while(false)
'''+block+'''
int main(){
 keys.begin_frame();keys.set(PKEY_UP,true);keys.set(PKEY_UP,false);AccumulateInput();
 for(int i=0;i<3;++i){keys.begin_frame();AccumulateInput();}
 CHECK(ConsumeInput()==INPUT_ROTATE);CHECK(ConsumeInput()==0);
 keys.begin_frame();keys.set(PKEY_SPACE,true);keys.set(PKEY_SPACE,false);AccumulateInput();
 keys.begin_frame();keys.cancel();AccumulateInput();keys.begin_frame();AccumulateInput();
 CHECK(ConsumeInput()==0);
 keys.begin_frame();keys.cancel();keys.set(PKEY_LEFT,true);keys.set(PKEY_LEFT,false);AccumulateInput();
 CHECK(ConsumeInput()==INPUT_LEFT);CHECK(ConsumeInput()==0);
 keys.begin_frame();keys.set(PKEY_RIGHT,true);AccumulateInput();CHECK(ConsumeInput()==INPUT_RIGHT);
 for(int i=0;i<7;++i)CHECK(ConsumeInput()==0);
 CHECK(ConsumeInput()==INPUT_RIGHT);
 keys.begin_frame();keys.cancel();keys.set(PKEY_RIGHT,true,true);AccumulateInput();CHECK(ConsumeInput()==0);
 keys.begin_frame();keys.set(PKEY_DOWN,true);AccumulateInput();CHECK(ConsumeInput()==INPUT_DOWN);CHECK(ConsumeInput()==INPUT_DOWN);
 keys.begin_frame();keys.set(PKEY_UP,true);AccumulateInput(true);CHECK(ConsumeInput(true)==0);
 std::puts("actual AccumulateInput/ConsumeInput: taps, cancellation, suppression and held/DAS passed");
}
''')
    binary=OUT/'root-input'
    run(['c++','-std=c++17','-DNDEBUG','-Wall','-Wextra','-Wpedantic','-I'+str(ROOT),str(harness),'-o',str(binary)])
    print(run([str(binary)]).stdout.strip(),flush=True)
    flags=['-std=c++17','-O1','-DNDEBUG','-fsanitize=undefined','-fno-sanitize-recover=all']
    run(['c++',*flags,'-I'+str(SOURCE),str(SOURCE/'tests/key_edges_contract.cpp'),'-o',str(OUT/'sanitized')])
    print(run([str(OUT/'sanitized')]).stdout.strip(),flush=True)
    for name, old, new in [
        ('lose_tap','held_[key] = down;','held_[key] = down; if (!down) pressed_[key] = false;'),
        ('repeat_revives','if (down && repeat) return;','(void)repeat;'),
        ('stale_cancel','pressed_[i] = false;\n        }\n        cancelled_ = true;','// incorrectly preserve press\n        }\n        cancelled_ = true;')
    ]:
        directory=OUT/name;(directory/'core').mkdir(parents=True,exist_ok=True)
        source=(SOURCE/'core/key_edges.h').read_text();assert old in source
        (directory/'core/key_edges.h').write_text(source.replace(old,new))
        run(['c++','-std=c++17','-DNDEBUG','-I'+str(directory),'-I'+str(SOURCE),str(SOURCE/'tests/key_edges_contract.cpp'),'-o',str(directory/'check')])
        result=subprocess.run([str(directory/'check')],capture_output=True,text=True,timeout=10)
        assert result.returncode==1 and 'CHECK failed' in result.stderr
        (directory/'result.log').write_text(result.stderr)
    print('97,656 event sequences; root adapters/input; SDL frame/tick pipeline; UBSan; three Release mutations passed.',flush=True)
if __name__=='__main__':main()
