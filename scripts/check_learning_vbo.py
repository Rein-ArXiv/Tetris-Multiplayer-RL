"""Check GL buffer ownership and real readback separately from drawing."""
from pathlib import Path
import json
import os
import subprocess

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / 'docs/learn/checkpoints/14-vbo'
OUT = ROOT / 'out/learning-checkpoints/14-vbo-check'


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
            assert not (build / 'tetris').exists() and not (build / 'buffer_probe').exists()
            run([str(build / 'input_demo')]); run([str(build / 'layout_demo')])
        else:
            env = {k:v for k,v in os.environ.items() if k not in ['LD_PRELOAD','LEARN_GL','LEARN_SMOKE']}
            result = run([str(build / 'buffer_probe')], env={**env, 'SDL_VIDEODRIVER':'offscreen'})
            for text in ['24 bytes read back', 'Unbind preserved storage',
                         '16 bytes and verified', 'no owned name remains']:
                assert text in result.stdout, result.stdout
            print(result.stdout.strip())
        print(f'{backend}: Release build and loader/mesh/buffer contracts passed')

    # Compare the loaded buffer signatures to Khronos headers on this host.
    abi = OUT / 'abi.cpp'
    types = {'GenBuffers':'PFNGLGENBUFFERSPROC', 'BindBuffer':'PFNGLBINDBUFFERPROC',
             'BufferData':'PFNGLBUFFERDATAPROC', 'DeleteBuffers':'PFNGLDELETEBUFFERSPROC',
             'GetBufferParameteriv':'PFNGLGETBUFFERPARAMETERIVPROC',
             'GetBufferSubData':'PFNGLGETBUFFERSUBDATAPROC'}
    abi.write_text('#include <GL/glcorearb.h>\n#include <type_traits>\n#include "renderer/gl_api.h"\n' +
                   '\n'.join(f'static_assert(std::is_same<decltype(study_gl::GlApi::{name}), {typ}>::value);'
                             for name,typ in types.items()) + '\nint main() {}\n')
    run(['c++', '-std=c++17', '-I' + str(SOURCE), str(abi), '-o', str(OUT / 'abi')])
    print('Six buffer signatures match installed Khronos types (host compiler)')

    # The ownership test must reject an implementation that forgets deletion.
    broken = OUT / 'missing_delete.cpp'
    source = (SOURCE / 'renderer/vertex_buffer.cpp').read_text()
    assert 'gl_.DeleteBuffers(1, &name_);' in source
    broken.write_text(source.replace('gl_.DeleteBuffers(1, &name_);', '// deliberate missing deletion'))
    run(['c++', '-std=c++17', '-DNDEBUG', '-I' + str(SOURCE), str(broken),
         str(SOURCE / 'tests/buffer_contract.cpp'), '-o', str(OUT / 'missing_delete')])
    failed = subprocess.run([str(OUT / 'missing_delete')], capture_output=True, text=True, timeout=10)
    assert failed.returncode == 1 and 'idempotent reset' in failed.stderr
    print('Negative control rejected missing deletion under NDEBUG')
    print('No draw/presentation performed; native Windows/macOS and hardware GPU remain untested.')


if __name__ == '__main__':
    main()
