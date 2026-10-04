"""Lesson168: target SDK selection, real native inference and relocated install.

Target-selection fixtures simulate CMake variables only. They do not validate
Windows/macOS/ARM compiler output. Runtime checks below run on Linux.
"""
from pathlib import Path
import argparse
import json
import os
import shutil
import subprocess
import sys
import tempfile
from check_learning_text_layout import run
from check_part_docs import normalized

R = Path(__file__).resolve().parents[1]
CP = R/'docs/learn/checkpoints/168-dependencies'
OUT = R/'out/learning-checkpoints/168-dependencies-check'


def layout_cases():
    module = R/'cmake/TetrisOnnxRuntime.cmake'
    with tempfile.TemporaryDirectory(prefix='study168 SDK ') as temp:
        root = Path(temp); sdk = root/'sdk'
        files = ['include/onnxruntime_c_api.h', 'include/onnxruntime_cxx_api.h',
                 'lib/linux-x64/libonnxruntime.so', 'lib/linux-aarch64/libonnxruntime.so',
                 'lib/osx-universal2/libonnxruntime.dylib', 'lib/win-x64/onnxruntime.dll',
                 'lib/win-x64/onnxruntime.lib']
        for name in files:
            path = sdk/name; path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text('Layout fixture, not an executable binary')
        cases = [
            ('Linux', 'x86_64', '', 8, '', True, 'linux-x64'),
            ('Linux', 'aarch64', '', 8, '', True, 'linux-aarch64'),
            ('Linux', 'riscv64', '', 8, '', False, ''),
            ('Linux', 'x86_64', '', 4, '', False, ''),
            ('Darwin', 'arm64', '', 8, 'arm64;x86_64', True, 'osx-universal2'),
            ('Darwin', 'arm64', '', 8, 'i386', False, ''),
            ('Windows', 'AMD64', 'x64', 8, '', True, 'win-x64'),
            ('Windows', 'ARM64', 'x64', 8, '', True, 'win-x64'),
            ('Windows', 'AMD64', 'ARM64', 8, '', False, ''),
            ('Windows', 'AMD64', 'ARM64EC', 8, '', False, ''),
            ('Windows', 'ARM64', '', 8, '', False, ''),
            ('Windows', 'AMD64', 'Win32', 4, '', False, ''),
            ('Other', 'x86_64', '', 8, '', False, ''),
        ]
        for index, (system, cpu, generator, pointer, mac, accepted, platform) in enumerate(cases):
            source = root/str(index);source.mkdir()
            # Change selection variables after native compiler configuration.
            (source/'CMakeLists.txt').write_text(f'''cmake_minimum_required(VERSION 3.15)
project(layout CXX)
set(CMAKE_SYSTEM_NAME "{system}")
set(CMAKE_SYSTEM_PROCESSOR "{cpu}")
set(CMAKE_GENERATOR_PLATFORM "{generator}")
set(CMAKE_SIZEOF_VOID_P {pointer})
set(CMAKE_OSX_ARCHITECTURES "{mac}")
include("{module}")
tetris_import_onnxruntime("{sdk}")
tetris_import_onnxruntime("{sdk}")
get_target_property(location Tetris::OnnxRuntime IMPORTED_LOCATION)
file(WRITE "${{CMAKE_BINARY_DIR}}/chosen.txt" "${{location}}")
''')
            command = ['cmake', '-S', str(source), '-B', str(source/'build')]
            result = subprocess.run(command, capture_output=True, text=True)
            assert (result.returncode == 0) == accepted, (index, result.stderr)
            if accepted:
                assert '/'+platform+'/' in (source/'build/chosen.txt').read_text()
        # Linux fixture: required link file, headers, and mixed roots fail early.
        source = root/'0'; cmake = source/'CMakeLists.txt'
        for relative in ['lib/linux-x64/libonnxruntime.so', 'include/onnxruntime_c_api.h']:
            path = sdk/relative; saved = path.read_bytes();path.unlink()
            result = subprocess.run(['cmake','-S',str(source),'-B',str(source/'build')],capture_output=True,text=True)
            assert result.returncode and 'missing' in result.stderr
            path.write_bytes(saved)
        cmake.write_text(cmake.read_text()+f'\ntetris_import_onnxruntime("{sdk}/other")\n')
        result = subprocess.run(['cmake','-S',str(source),'-B',str(source/'build')],capture_output=True,text=True)
        assert result.returncode and 'cannot mix SDK roots' in ' '.join(result.stderr.split()), result.stderr
        print('SDK target selection and missing/mixed inputs rejected; cross-OS configuration only', flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--python', default=sys.executable)
    parser.add_argument('--snippets-only', action='store_true')
    args = parser.parse_args()
    previous = CP.with_name('167-build-targets')
    changed = {'README.md', 'inference/CMakeLists.txt'}
    for path in previous.rglob('*'):
        relative = path.relative_to(previous)
        if path.is_file() and '__pycache__' not in path.parts and relative.as_posix() not in changed:
            assert path.read_bytes() == (CP/relative).read_bytes(), relative
    assert (CP/'dependencies/OnnxRuntime.cmake').read_bytes() == (R/'cmake/TetrisOnnxRuntime.cmake').read_bytes()
    OUT.mkdir(parents=True, exist_ok=True)
    if not args.snippets_only:
        layout_cases()
        if not sys.platform.startswith('linux'):
            parser.error('Relocation experiment needs Linux; run target-platform validation separately')
        pybind = run([args.python,'-m','pybind11','--cmakedir']).stdout.strip()
        training = OUT/'training'
        print(run(['cmake','-S',str(CP/'roles'),'-B',str(training),'-DSTUDY_ROLE=TRAINING',
                   '-DCMAKE_BUILD_TYPE=Release','-DPython_EXECUTABLE='+args.python,'-Dpybind11_DIR='+pybind]).stdout[-200:])
        result = run(['cmake','--build',str(training),'--parallel','2'],timeout=300)
        (OUT/'training-build.log').write_text(result.stdout+result.stderr)
        print(run(['ctest','--test-dir',str(training),'-C','Release','--output-on-failure'],timeout=300).stdout,flush=True)
        with tempfile.TemporaryDirectory(prefix='study168 relocation ') as temp:
            folder = Path(temp);sdk = folder/'development SDK'
            shutil.copytree(R/'third_party/onnxruntime',sdk)
            build = folder/'build';installed = folder/'install'
            run(['cmake','-S',str(CP/'roles'),'-B',str(build),'-DSTUDY_ROLE=POLICY',
                 '-DCMAKE_BUILD_TYPE=Release','-DORT_ROOT='+str(sdk)])
            result = run(['cmake','--build',str(build),'--parallel','2'],timeout=300)
            (OUT/'policy-build.log').write_text(result.stdout+result.stderr)
            print(run(['ctest','--test-dir',str(build),'-C','Release','--output-on-failure']).stdout)
            run(['cmake','--install',str(build),'--config','Release','--prefix',str(installed)])
            moved = folder/'relocated tree';installed.rename(moved)
            shutil.rmtree(sdk);shutil.rmtree(build)
            executable = moved/'bin/dependency_probe'
            dynamic = run(['readelf','-d',str(executable)]).stdout
            assert '$ORIGIN/../lib' in dynamic and str(folder/'development SDK') not in dynamic
            environment = dict(os.environ);environment.pop('LD_LIBRARY_PATH',None);environment.pop('LD_PRELOAD',None)
            result = subprocess.run([str(executable)],cwd=folder,env=environment,capture_output=True,text=True)
            assert result.returncode == 0, result.stderr
            print('Relocated:',result.stdout.strip(),flush=True)
            result = subprocess.run([args.python,str(CP/'tests/policy_match_inference.py'),
                                     '--module-dir',str(training/'bindings/python/Release'),
                                     '--probe',str(moved/'bin/policy_match_probe')],
                                    cwd=folder,env=environment,capture_output=True,text=True,timeout=180)
            assert result.returncode == 0, result.stdout+result.stderr
            print(result.stdout,flush=True)
            # No SDK/build fallback remains: missing bundled SONAME must fail.
            libraries = moved/'lib';hidden = folder/'hidden-libraries';libraries.rename(hidden)
            result = subprocess.run([str(executable)],cwd=folder,env=environment,capture_output=True,text=True)
            assert result.returncode != 0 and 'libonnxruntime' in result.stderr
            hidden.rename(libraries)
            print('Missing installed runtime rejected by loader',flush=True)
    lesson = R/'docs/learn/lessons/168.json'
    if lesson.exists():
        corpus = {'cpp':[normalized(p.read_text(),'cpp') for p in CP.rglob('*.cpp')],
                  'cmake':[normalized(p.read_text(),'cmake') for p in CP.rglob('*.cmake')]+
                           [normalized(p.read_text(),'cmake') for p in CP.rglob('CMakeLists.txt')]}
        count = 0
        for section in json.loads(lesson.read_text())['sections']:
            reference = section.get('reference')
            if reference:assert reference['symbol'] in (R/reference['path']).read_text(),reference
            for code in section.get('codes',[]):
                language = code.get('language')
                if 'text' in code and language in corpus:
                    assert any(normalized(code['text'],language) in source for source in corpus[language]),code['label']
                    count += 1
        print('Inline snippets:',count,'current references and cumulative preservation passed')

if __name__ == '__main__':main()
