"""Validate reviewed traces, failure diagnostics and production reference gates."""
from pathlib import Path
import importlib.util,json,os,subprocess,sys
ROOT=Path(__file__).resolve().parents[1]
SOURCE=ROOT/'docs/learn/checkpoints/53-golden-regression'
OUT=ROOT/'out/learning-checkpoints/53-golden-regression-check'
def run(args,**kw):
    r=subprocess.run(args,cwd=ROOT,text=True,capture_output=True,timeout=kw.pop('timeout',360),**kw)
    if r.returncode:raise RuntimeError(f'{args}\n{r.stdout}\n{r.stderr}')
    return r

def main():
    OUT.mkdir(parents=True,exist_ok=True)
    old=ROOT/'docs/learn/checkpoints/52-state-hash'
    for p in old.rglob('*'):
        rel=p.relative_to(old)
        if p.is_file() and str(rel) not in {'CMakeLists.txt','README.md'}:assert p.read_bytes()==(SOURCE/rel).read_bytes()
    env={**os.environ,'SDL_VIDEODRIVER':'dummy','DISPLAY':'','WAYLAND_DISPLAY':''}
    check=SOURCE/'tools/check_golden.py'
    for backend in ['SCRIPTED','SDL']:
        build=OUT/backend.lower()
        run(['cmake','-S',str(SOURCE),'-B',str(build),'-DSTUDY_PLATFORM='+backend,'-DCMAKE_BUILD_TYPE=Release','-DCMAKE_CXX_FLAGS=-Wall -Wextra -Wpedantic'])
        r=run(['cmake','--build',str(build),'-j3']);(OUT/f'build-{backend}.log').write_text(r.stdout+r.stderr);assert 'warning:' not in r.stdout+r.stderr
        r=run(['ctest','--test-dir',str(build),'--output-on-failure'],env=env);(OUT/f'ctest-{backend}.log').write_text(r.stdout);print(r.stdout.strip(),flush=True)
        for scenario in ['mixed-v1','overflow-v1']:
            output=run([str(build/'golden_trace'),scenario,'1']).stdout
            assert json.loads(output)==json.loads((SOURCE/'golden'/f'{scenario}.json').read_text())
            (OUT/f'{backend}-{scenario}.json').write_text(output)
    binary=OUT/'scripted/golden_trace'
    spec=importlib.util.spec_from_file_location('golden',check);module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
    for seed in [0,1,42,0xffffffffffffffff]:
        for scenario in ['mixed-v1','overflow-v1']:
            a=run([str(binary),scenario,str(seed)]).stdout;b=run([str(binary),scenario,str(seed)]).stdout
            assert a==b;module.parse_trace(a.encode())
    candidate=OUT/'candidate-exclusive.json'
    if candidate.exists():candidate.unlink() # Test-owned disposable output only.
    run([sys.executable,str(check),'capture',str(binary),str(candidate)])
    before=candidate.read_bytes()
    r=subprocess.run([sys.executable,str(check),'capture',str(binary),str(candidate)],capture_output=True,text=True)
    assert r.returncode==2 and candidate.read_bytes()==before
    baseline=SOURCE/'golden/mixed-v1.json';before=baseline.read_bytes()
    r=subprocess.run([sys.executable,str(check),'capture',str(binary),str(baseline)],capture_output=True,text=True)
    assert r.returncode==2 and baseline.read_bytes()==before
    for name,data in [('empty',b''),('malformed',b'{'),('duplicate',b'{"records":[],"records":[]}')]:
        p=OUT/f'{name}.json';p.write_bytes(data)
        r=subprocess.run([sys.executable,str(check),'check',str(p),str(binary)],text=True,capture_output=True)
        assert r.returncode==2 and 'cannot compare' in r.stderr
    for name,path,before,after,code,diagnostic in [
        ('rule_counter','simulation/gravity.h','int elapsed = 0;','int elapsed = 1;',1,'records[0].bytes offset='),
        ('input_script','src/golden_trace.cpp','{0,0},{0,0},{1,0}','{1,0},{0,0},{1,0}',1,'records[1].mask'),
        ('byte_schema','simulation/state_hash.h','out.u32(1);','out.u32(2);',2,'wrong byte schema tag')]:
        folder=OUT/name;p=folder/path;p.parent.mkdir(parents=True,exist_ok=True)
        text=(SOURCE/path).read_text();assert before in text;p.write_text(text.replace(before,after))
        source=p if path.startswith('src/') else SOURCE/'src/golden_trace.cpp'
        run(['c++','-std=c++17','-O2','-DNDEBUG','-I'+str(folder),'-I'+str(SOURCE),str(source),'-o',str(folder/'trace')])
        r=subprocess.run([sys.executable,str(check),'check',str(baseline),str(folder/'trace')],text=True,capture_output=True)
        assert r.returncode==code and diagnostic in r.stdout+r.stderr,(name,r.returncode,r.stdout,r.stderr)
        (folder/'result.log').write_text(r.stdout+r.stderr)
    broken=OUT/'failed-emitter';broken.write_text('#!/bin/sh\nprintf "{\\n"\nexit 1\n');broken.chmod(0o700)
    bad_candidate=OUT/'must-not-exist.json'
    r=subprocess.run([sys.executable,str(check),'capture',str(broken),str(bad_candidate)],text=True,capture_output=True)
    assert r.returncode==2 and not bad_candidate.exists()
    for args in [['bad','1'],['mixed-v1','-1'],['mixed-v1','18446744073709551616'],['mixed-v1','1','extra']]:
        r=subprocess.run([str(binary),*args],text=True,capture_output=True);assert r.returncode==2 and not r.stdout
    with open('/dev/full','wb')as output:
        assert subprocess.run([str(binary)],stdout=output,stderr=subprocess.PIPE).returncode==1
    root=OUT/'root';python=ROOT/'.venv/bin/python'
    cmake_dir=run([str(python),'-m','pybind11','--cmakedir']).stdout.strip()
    run(['cmake','-S',str(ROOT),'-B',str(root),'-DCMAKE_BUILD_TYPE=Release','-DTETRIS_BUILD_GAME=OFF','-DTETRIS_BUILD_REACTOR=OFF','-DTETRIS_BUILD_PY=ON','-Dpybind11_DIR='+cmake_dir,'-DPython_EXECUTABLE='+str(python)])
    r=run(['cmake','--build',str(root),'--target','sim_hash_dump','tetris_py','-j3']);(OUT/'root-build-final.log').write_text(r.stdout+r.stderr)
    output=run([str(root/'sim_hash_dump')]).stdout;assert output==(ROOT/'python/tests/_sim_hash_dump.txt').read_text()
    for args in [['-1'],['abc'],['1x'],['18446744073709551616'],['0x10000000000000000'],['08'],['1','bad'],['+1'],[' 1']]:
        r=subprocess.run([str(root/'sim_hash_dump'),*args],text=True,capture_output=True);assert r.returncode==2 and not r.stdout,args
    assert run([str(root/'sim_hash_dump'),'8']).stdout==run([str(root/'sim_hash_dump'),'010']).stdout==run([str(root/'sim_hash_dump'),'0x8']).stdout
    with open('/dev/full','wb')as output:
        assert subprocess.run([str(root/'sim_hash_dump')],stdout=output).returncode==1
    native_env={**os.environ,'PYTHONPATH':str(root)+os.pathsep+str(ROOT/'python')}
    script='import tetris_py,pytest,sys; print(tetris_py.__file__); assert "53-golden-regression-check/root" in tetris_py.__file__; sys.exit(pytest.main(["python/tests/test_determinism_reference.py","python/tests/test_determinism_crossplatform.py","-q"]))'
    r=run([str(python),'-c',script],env=native_env);(OUT/'native-reference-tests.log').write_text(r.stdout+r.stderr);print(r.stdout.strip(),flush=True)
    run(['c++','-std=c++17','-O1','-fsanitize=undefined','-fno-sanitize-recover=all','-I'+str(SOURCE),str(SOURCE/'src/golden_trace.cpp'),'-o',str(OUT/'sanitized')])
    for scenario in ['mixed-v1','overflow-v1']:run([sys.executable,str(check),'check',str(SOURCE/'golden'/f'{scenario}.json'),str(OUT/'sanitized')])
    print('Trace suites, read-only comparison, exclusive capture, independent parser cases, rule/input/schema mutations, CLI/output failures, fresh native parity and UBSan passed.',flush=True)
if __name__=='__main__':main()
