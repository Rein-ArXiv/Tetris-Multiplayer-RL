"""Check program interfaces, ownership and selection."""
from pathlib import Path
import json
import os
import subprocess

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / 'docs/learn/checkpoints/17-program'
OUT = ROOT / 'out/learning-checkpoints/17-program-check'


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
            assert not (build / 'tetris').exists() and not (build / 'program_probe').exists()
            run([str(build / 'input_demo')]); run([str(build / 'layout_demo')])
        else:
            env = {k:v for k,v in os.environ.items() if k not in ['LD_PRELOAD','LEARN_GL','LEARN_SMOKE']}
            for inherited in ['buffer_probe','vao_probe','shader_probe']:
                run([str(build / inherited)], env={**env, 'SDL_VIDEODRIVER':'offscreen'})
            result = run([str(build / 'program_probe')], env={**env, 'SDL_VIDEODRIVER':'offscreen'})
            for text in ['mismatch: LINK_STATUS false while GetError is zero',
                         'preserves the currently selected program', 'zero attachments',
                         'Selection cleared before idempotent program deletion']:
                assert text in result.stdout, result.stdout
            print(result.stdout.strip())
        print(f'{backend}: Release build and six loader/mesh/buffer/VAO/shader/program contracts passed')

    # Compare the loaded buffer signatures to Khronos headers on this host.
    abi = OUT / 'abi.cpp'
    types = {name: 'PFNGL'+name.upper()+'PROC' for name in [
        'CreateProgram','AttachShader','LinkProgram','GetProgramiv','GetProgramInfoLog',
        'DetachShader','DeleteProgram','UseProgram']}
    abi.write_text('#include <GL/glcorearb.h>\n#include <type_traits>\n#include "renderer/gl_api.h"\n' +
                   '\n'.join(f'static_assert(std::is_same<decltype(study_gl::GlApi::{name}), {typ}>::value);'
                             for name,typ in types.items()) + '\nint main() {}\n')
    run(['c++', '-std=c++17', '-I' + str(SOURCE), str(abi), '-o', str(OUT / 'abi')])
    print('Eight program signatures match installed Khronos types (host compiler)')

    # The ownership test must reject an implementation that forgets deletion.
    broken = OUT / 'missing_delete.cpp'
    source = (SOURCE / 'renderer/program.cpp').read_text()
    assert 'gl_.DeleteProgram(name_);' in source
    broken.write_text(source.replace('gl_.DeleteProgram(name_);', '// deliberate missing deletion'))
    run(['c++', '-std=c++17', '-DNDEBUG', '-I' + str(SOURCE), str(broken),
         str(SOURCE / 'tests/program_contract.cpp'), '-o', str(OUT / 'missing_delete')])
    failed = subprocess.run([str(OUT / 'missing_delete')], capture_output=True, text=True, timeout=10)
    assert failed.returncode == 1 and 'idempotent reset' in failed.stderr
    # Compile the real production helper with API doubles; do not mirror its implementation.
    production=(ROOT/'renderer/renderer.cpp').read_text()
    start=production.index('static GLuint link_program(')
    end=production.index('// ─── 배처',start)
    helper=production[start:end]
    fixture=(ROOT/'tests/learning/current_program_template.cpp').read_text()
    current=OUT/'current_program.cpp'
    current.write_text(fixture.replace('// @PRODUCTION_LINK_PROGRAM@',helper))
    run(['c++','-std=c++17','-DNDEBUG','-I'+str(SOURCE),str(current),'-o',str(OUT/'current_program')])
    print(run([str(OUT/'current_program')]).stdout.strip())
    print('Negative control rejected missing program deletion under NDEBUG')
    print('No draw/presentation performed; native Windows/macOS and hardware GPU remain untested.')


if __name__ == '__main__':
    main()
