"""Check the drawing pass, framebuffer pixels and platform presentation boundary."""
from pathlib import Path
import json
import os
import subprocess
import shlex

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / 'docs/learn/checkpoints/18-triangle'
OUT = ROOT / 'out/learning-checkpoints/18-triangle-check'


def run(args, **kwargs):
    result = subprocess.run(args, cwd=ROOT, text=True, capture_output=True, timeout=60, **kwargs)
    if result.returncode:
        raise RuntimeError(f'{args}\n{result.stdout}\n{result.stderr}')
    return result


def main():
    for backend in ['SCRIPTED', 'SDL']:
        build = OUT / backend.lower()
        run(['cmake', '-S', str(SOURCE), '-B', str(build), '-DSTUDY_PLATFORM=' + backend,
             '-DCMAKE_BUILD_TYPE=Release', '-DCMAKE_EXPORT_COMPILE_COMMANDS=ON',
             '-DCMAKE_CXX_FLAGS=-Wall -Wextra -Wpedantic'])
        result = run(['cmake', '--build', str(build), '-j2'])
        assert 'warning:' not in result.stderr, result.stderr
        run(['ctest', '--test-dir', str(build), '--output-on-failure'])
        commands = json.loads((build / 'compile_commands.json').read_text())
        for row in commands:
            if '/renderer/' in row['file']:
                assert 'SDL2' not in row['command'], row
        if backend == 'SCRIPTED':
            assert not (build / 'tetris').exists() and not (build / 'triangle_probe').exists()
            run([str(build / 'input_demo')]); run([str(build / 'layout_demo')])
        else:
            env = {k:v for k,v in os.environ.items() if k not in ['LD_PRELOAD','LEARN_GL','LEARN_SMOKE']}
            for inherited in ['buffer_probe','vao_probe','shader_probe','program_probe']:
                run([str(build / inherited)], env={**env, 'SDL_VIDEODRIVER':'offscreen'})
            result = run([str(build / 'triangle_probe'), str(OUT/'triangle.ppm')], env={**env, 'SDL_VIDEODRIVER':'offscreen'})
            for text in ['Triangle interior and background pixels match', 'Two vertices:',
                         'Clear after draw erases', 'Zero viewport:', 'Next pass resets viewport']:
                assert text in result.stdout, result.stdout
            print(result.stdout.strip())
        print(f'{backend}: Release build and seven loader/mesh/buffer/VAO/shader/program/triangle contracts passed')

    # Compare the loaded buffer signatures to Khronos headers on this host.
    abi = OUT / 'abi.cpp'
    types = {name: 'PFNGL'+name.upper()+'PROC' for name in [
        'Viewport','ClearColor','Clear','DrawArrays','ReadPixels','ReadBuffer']}
    abi.write_text('#include <GL/glcorearb.h>\n#include <type_traits>\n#include "renderer/gl_api.h"\n' +
                   '\n'.join(f'static_assert(std::is_same<decltype(study_gl::GlApi::{name}), {typ}>::value);'
                             for name,typ in types.items()) + '\nint main() {}\n')
    run(['c++', '-std=c++17', '-I' + str(SOURCE), str(abi), '-o', str(OUT / 'abi')])
    print('Six draw/readback signatures match installed Khronos types (host compiler)')

    # A mutation that treats byte count as vertex count must fail the contract.
    broken = OUT / 'wrong_count.cpp'
    source = (SOURCE / 'renderer/triangle.cpp').read_text()
    assert 'gl.DrawArrays(Triangles, 0, 3);' in source
    broken.write_text(source.replace('gl.DrawArrays(Triangles, 0, 3);','gl.DrawArrays(Triangles, 0, 24);'))
    run(['c++','-std=c++17','-DNDEBUG','-I'+str(SOURCE),str(broken),
         str(SOURCE/'tests/triangle_contract.cpp'),'-o',str(OUT/'wrong_count')])
    failed=subprocess.run([str(OUT/'wrong_count')],capture_output=True,text=True,timeout=10)
    assert failed.returncode==1 and 'vertex count, not byte count' in failed.stderr
    print('Negative control rejected byte count used as vertex count under NDEBUG')

    # SDL-call doubles verify pixel-unit queries and presentation gating only.
    flags=shlex.split(run(['pkg-config','--cflags','--libs','sdl2']).stdout)
    probe=OUT/'drawable_probe.so'
    run(['cc','-shared','-fPIC','-Wall','-Wextra',str(ROOT/'tests/learning/drawable_probe.c'),
         *flags,'-ldl','-o',str(probe)])
    binary=OUT/'drawable_contract'
    run(['c++','-std=c++17','-I'+str(SOURCE),str(ROOT/'tests/learning/platform_drawable.cpp'),
         str(SOURCE/'platform/sdl.cpp'),*flags,'-o',str(binary)])
    base={k:v for k,v in os.environ.items() if k not in ['LD_PRELOAD','LEARN_GL','SDL_VIDEODRIVER']}
    for mode in ['okay','zero-drawable','minimized']:
        r=run([str(binary),*([] if mode=='okay' else ['empty'])],
              env={**base,'SDL_VIDEODRIVER':'dummy','LD_PRELOAD':str(probe),'LEARN_GL':mode})
        assert r.stderr.count('drawprobe:swap')==(1 if mode=='okay' else 0),r.stderr
    print('Platform doubles: actual-pixel units, unavailable surfaces, inactive lifetime and swap boundary passed')
    # Extract the actual production frame-begin helper and test zero -> positive transitions.
    production=(ROOT/'renderer/renderer.cpp').read_text()
    start=production.index('void renderer_begin(Color bg)')
    end=production.index('void renderer_set_view_offset(',start)
    fixture=(ROOT/'tests/learning/current_frame_template.cpp').read_text()
    current=OUT/'current_frame.cpp'
    current.write_text(fixture.replace('// @PRODUCTION_RENDERER_BEGIN@',production[start:end]))
    run(['c++','-std=c++17','-DNDEBUG',str(current),'-o',str(OUT/'current_frame')])
    print(run([str(OUT/'current_frame')]).stdout.strip())
    # Native backend evidence is optional; distinguish unavailable display from readback.
    native=subprocess.run([str(OUT/'sdl/triangle_probe')],cwd=ROOT,text=True,capture_output=True,
                          env={**base,'SDL_VIDEODRIVER':'x11'},timeout=20)
    if native.returncode==0:
        print('Native X11 hidden-window probe: passed')
    elif 'SDL init: x11 not available' in native.stderr:
        print('Native X11 hidden-window probe: unavailable: '+native.stderr.strip())
    else:
        raise RuntimeError('Native probe failed after a different boundary; do not count as unavailable: '+native.stderr)

    print('No physical GPU, visible presentation or native Windows/macOS validation claimed.')

if __name__ == '__main__':
    main()
