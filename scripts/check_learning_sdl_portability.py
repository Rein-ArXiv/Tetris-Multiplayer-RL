"""Audit cumulative SDL entry points and syntax-check distinct SDL translation units.

Native Linux syntax and package-path checks do not prove Windows/macOS execution.
"""
from pathlib import Path
import hashlib,re,subprocess,json
from check_learning_text_layout import run
R=Path(__file__).resolve().parents[1];BASE=R/'docs/learn/checkpoints';OUT=R/'out/learning-checkpoints/review-sdl'
def main():
    OUT.mkdir(parents=True,exist_ok=True);unique={};audited=0
    for cp in sorted(BASE.iterdir()):
        if not cp.is_dir():continue
        cm=(cp/'CMakeLists.txt').read_text()
        assert 'find_package(SDL2 CONFIG QUIET)'in cm,cp
        assert 'target_compile_definitions(StudySDL2 INTERFACE SDL_MAIN_HANDLED)'in cm,cp
        for f in cp.rglob('*.cpp'):
            s=f.read_text()
            if '#include <SDL.h>'not in s:continue
            if re.search(r'\bint main\s*\(',s):
                assert s.index('SDL_MAIN_HANDLED')<s.index('#include <SDL.h>'),f
            if 'SDL_Init('in s:assert 'SDL_SetMainReady'in s,f
            if 'SDL_GL_CONTEXT_PROFILE_CORE'in s:assert 'SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG'in s,f
            unique.setdefault(hashlib.sha256(s.encode()).hexdigest(),(cp,f));audited+=1
    flags=run(['pkg-config','--cflags','sdl2']).stdout.split()
    # Full file contents differ whenever dependent teaching code normally changes.
    # Every selected file is still compiled against its own checkpoint headers.
    for cp,f in unique.values():
        result=run(['c++','-std=c++17','-fsyntax-only','-DSDL_MAIN_HANDLED','-I'+str(cp),'-I'+str(R),*flags,str(f)],timeout=60)
    for label,extra in [('config',['-DCMAKE_DISABLE_FIND_PACKAGE_PkgConfig=TRUE']),('pkg',['-DCMAKE_DISABLE_FIND_PACKAGE_SDL2=TRUE'])]:
        b=OUT/label
        run(['cmake','-S',str(BASE/'02-window'),'-B',str(b),'-DCMAKE_EXPORT_COMPILE_COMMANDS=ON',*extra])
        run(['cmake','--build',str(b),'--target','tetris','-j2'])
        assert all('SDL_MAIN_HANDLED'in c['command']for c in json.loads((b/'compile_commands.json').read_text()))
    print('SDL source audit',audited,'distinct syntax checks',len(unique),'Config/pkg fallback builds passed')
if __name__=='__main__':main()
