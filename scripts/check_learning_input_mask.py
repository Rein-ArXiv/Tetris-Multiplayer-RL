"""Validate the mask checkpoint, conversions and root replay boundaries."""
from pathlib import Path
import os
import subprocess
ROOT=Path(__file__).resolve().parents[1]
SOURCE=ROOT/'docs/learn/checkpoints/48-input-mask'
OUT=ROOT/'out/learning-checkpoints/48-input-mask-check'
def run(args,**kwargs):
    result=subprocess.run(args,cwd=kwargs.pop('cwd',ROOT),text=True,capture_output=True,
                          timeout=kwargs.pop('timeout',240),**kwargs)
    if result.returncode:raise RuntimeError(f'{args}\n{result.stdout}\n{result.stderr}')
    return result

def main():
    OUT.mkdir(parents=True,exist_ok=True)
    old=ROOT/'docs/learn/checkpoints/47-input-edges'
    for path in old.rglob('*'):
        if path.is_file() and str(path.relative_to(old)) not in {'simulation/pending_controls.h','README.md','CMakeLists.txt'}:
            assert path.read_bytes()==(SOURCE/path.relative_to(old)).read_bytes()
    env={**os.environ,'SDL_VIDEODRIVER':'dummy','DISPLAY':'','WAYLAND_DISPLAY':''}
    for backend in ['SCRIPTED','SDL']:
        build=OUT/backend.lower()
        run(['cmake','-S',str(SOURCE),'-B',str(build),'-DSTUDY_PLATFORM='+backend,'-DCMAKE_BUILD_TYPE=Release','-DCMAKE_CXX_FLAGS=-Wall -Wextra -Wpedantic'])
        targets=[] if backend=='SCRIPTED' else ['--target','tetris','mask_demo','mask_contract','frame_contract','input_pipeline','key_edges_contract']
        result=run(['cmake','--build',str(build),'-j3',*targets],timeout=360)
        (OUT/f'build-{backend}.log').write_text(result.stdout+result.stderr)
        assert 'warning:' not in result.stdout+result.stderr
        pattern=[] if backend=='SCRIPTED' else ['-R','^(mask_contract|frame_contract|input_pipeline|key_edges_contract)$']
        result=run(['ctest','--test-dir',str(build),'--output-on-failure',*pattern],env=env)
        (OUT/f'ctest-{backend}.log').write_text(result.stdout);print(result.stdout.strip(),flush=True)
        text=run([str(build/'mask_demo')],env=env).stdout
        (OUT/f'mask_demo-{backend}.log').write_text(text)
        for row in ['raw=9 bits=00001001 h=-1 cw=1 soft=0 hard=0','raw=19 bits=00010011 h=0 cw=0 soft=0 hard=1','raw=31 bits=00011111 h=0 cw=1 soft=1 hard=1','raw=32 invalid','raw=256 invalid','encode(decode(left|right))=0','t2 h=0 soft=1','t3 h=0 soft=0']:
            assert row in text,row
        link=(build/'CMakeFiles/mask_demo.dir/link.txt').read_text();assert 'SDL' not in link and 'study_platform' not in link
    flags=['-std=c++17','-O1','-DNDEBUG','-fsanitize=undefined','-fno-sanitize-recover=all']
    for name, sources, include in [
        ('mask-sanitized',[SOURCE/'tests/mask_contract.cpp'],SOURCE),
        ('replay-sanitized',[ROOT/'tests/replay_io_test.cpp',ROOT/'core/replay.cpp'],ROOT)]:
        run(['c++',*flags,'-I'+str(include),*[str(s) for s in sources],'-o',str(OUT/name)])
        print(run([str(OUT/name)],cwd=OUT).stdout.strip(),flush=True)
    for name,path,old,new in [
        ('early_narrow','simulation/input_mask.h','return (value & ~static_cast<unsigned>(known)) == 0u;','return (static_cast<Mask>(value) & ~static_cast<unsigned>(known)) == 0u;'),
        ('xor_capture','simulation/pending_controls.h','pending_ |= edges;','pending_ ^= edges;'),
        ('held_latched','simulation/pending_controls.h','soft_drop_ = downHeld;','soft_drop_ = soft_drop_ || downHeld;')]:
        mutation=OUT/name;p=mutation/path;p.parent.mkdir(parents=True,exist_ok=True)
        text=(SOURCE/path).read_text();assert old in text;p.write_text(text.replace(old,new))
        run(['c++','-std=c++17','-DNDEBUG','-I'+str(mutation),'-I'+str(SOURCE),str(SOURCE/'tests/mask_contract.cpp'),'-o',str(mutation/'check')])
        result=subprocess.run([str(mutation/'check')],capture_output=True,text=True,timeout=10)
        assert result.returncode==1 and 'CHECK failed' in result.stderr
        (mutation/'result.log').write_text(result.stderr)
    print('Masks: 65,536 raw values, all 65,536 bit queries, 32,768 frame histories; root strict replay; UBSan and three Release mutations passed.',flush=True)
if __name__=='__main__':main()
