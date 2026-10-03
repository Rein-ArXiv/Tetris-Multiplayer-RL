"""Lesson 71 character art integration; manuscript review is separate."""
from pathlib import Path
import os, subprocess
from check_learning_text_layout import run
from check_learning_utf8 import cut
ROOT=Path(__file__).resolve().parents[1]
SOURCE=ROOT/'docs/learn/checkpoints/71-character-art'
OUT=ROOT/'out/learning-checkpoints/71-character-art-check'
def content_variants():
 original=(SOURCE/'content/characters.h').read_text()
 player='    {"player", u8"플레이어", "assets/player.png", "assets/player.png"},'
 rook='    {"rook", u8"루크", "assets/bot.png", "assets/opponent.png"},'
 assert player in original and rook in original
 probe=OUT/'identity-probe.cpp'
 probe.write_text(r'''#include "client/menu_model.h"
#include "content/art_set.h"
#include <cstdio>
#include <cstdlib>
#define CHECK(e) do{if(!(e))std::abort();}while(false)
int main(){study_menu::Preferences p;CHECK(p.character_id()=="player");CHECK(p.select_character("rook"));CHECK(p.badge()==EXPECT_INDEX);
study_art::ArtSet art;CHECK(art.init([](const std::string& path)->study_art::Handle{return path=="assets/player.png"?101:path=="assets/bot.png"?202:303;},99));
CHECK(art.resolve(p.character_id())->icon==202&&art.resolve(p.character_id())->portrait==303);
std::printf("Selected rook, index %zu, correct icon and portrait after catalog order change\n",p.badge());}''')
 for name,source,index in [('original',original,1),('reordered',original.replace(player,'__PLAYER__').replace(rook,player).replace('__PLAYER__',rook),0)]:
  directory=OUT/name;(directory/'content').mkdir(parents=True,exist_ok=True);(directory/'content/characters.h').write_text(source)
  binary=directory/'identity-probe'
  run(['c++','-std=c++17','-O1','-Wall','-Wextra','-Wpedantic','-DEXPECT_INDEX='+str(index),'-I'+str(directory),'-I'+str(SOURCE),str(probe),'-o',str(binary)])
  result=run([str(binary)]);(directory/'result.log').write_text(result.stdout);print(result.stdout.strip(),flush=True)
 directory=OUT/'duplicate';(directory/'content').mkdir(parents=True,exist_ok=True)
 (directory/'content/characters.h').write_text(original.replace('{"rook",','{"player",'))
 result=subprocess.run(['c++','-std=c++17','-DEXPECT_INDEX=1','-I'+str(directory),'-I'+str(SOURCE),str(probe),'-o',str(directory/'rejected')],cwd=ROOT,capture_output=True,text=True)
 assert result.returncode!=0 and 'character catalog must be valid' in result.stderr
 (directory/'compile.log').write_text(result.stderr);print('Duplicate character ID rejected at compile time',flush=True)

def main():
 OUT.mkdir(parents=True,exist_ok=True)
 previous=SOURCE.parent/'70-pointer-mapping'
 allowed={'CMakeLists.txt','README.md','DESIGN.md','src/main.cpp','client/menu_model.h','renderer/menu_labels.h','renderer/menu_controls.h','tests/widget_render.cpp'}
 for p in previous.rglob('*'):
  if p.is_file() and p.relative_to(previous).as_posix() not in allowed:
   assert p.read_bytes()==(SOURCE/p.relative_to(previous)).read_bytes(),p
 env={**os.environ,'SDL_VIDEODRIVER':'dummy','SDL_AUDIODRIVER':'dummy','DISPLAY':'','WAYLAND_DISPLAY':''}
 for backend in ['SCRIPTED','SDL']:
  build=OUT/backend.lower()
  run(['cmake','-S',str(SOURCE),'-B',str(build),'-DSTUDY_PLATFORM='+backend,'-DCMAKE_BUILD_TYPE=Release','-DCMAKE_CXX_FLAGS=-Wall -Wextra -Wpedantic'])
  result=run(['cmake','--build',str(build),'-j3']);(OUT/f'build-{backend}.log').write_text(result.stdout+result.stderr);assert 'warning:' not in result.stdout+result.stderr
  result=run(['ctest','--test-dir',str(build),'--output-on-failure'],env=env);(OUT/f'ctest-{backend}.log').write_text(result.stdout);print(result.stdout[-350:],flush=True)
 result=run([str(OUT/'sdl/pointer_sdl')],env=env);(OUT/'pointer-sdl.log').write_text(result.stdout+result.stderr);print(result.stdout.strip(),flush=True)
 result=run([str(OUT/'sdl/widget_render'),str(SOURCE/'assets/NanumGothic.ttf')],env={**os.environ,'SDL_VIDEODRIVER':'offscreen','LIBGL_ALWAYS_SOFTWARE':'1'});(OUT/'widget-render.log').write_text(result.stdout+result.stderr);print(result.stdout.strip(),flush=True)
 result=run([str(OUT/'sdl/pointer_pixels')],env={**os.environ,'SDL_VIDEODRIVER':'offscreen','LIBGL_ALWAYS_SOFTWARE':'1'});(OUT/'pointer-pixels.log').write_text(result.stdout+result.stderr);print(result.stdout.strip(),flush=True)
 result=run([str(OUT/'sdl/character_art_real'),str(SOURCE)],env={**os.environ,'SDL_VIDEODRIVER':'offscreen','LIBGL_ALWAYS_SOFTWARE':'1'});(OUT/'character-art-real.log').write_text(result.stdout+result.stderr);print(result.stdout.strip(),flush=True)
 binary=OUT/'ui-sanitized';run(['c++','-std=c++17','-O1','-Wall','-Wextra','-Wpedantic','-fsanitize=undefined,float-cast-overflow','-fno-sanitize-recover=all','-I'+str(SOURCE),str(SOURCE/'tests/character_art_contract.cpp'),'-o',str(binary)])
 result=run([str(binary)]);(OUT/'sanitized.log').write_text(result.stdout);print(result.stdout.strip(),flush=True)
 content_variants()
 result=run(['python3',str(ROOT/'scripts/check_learning_art_root.py')]);(OUT/'root-art.log').write_text(result.stdout);print(result.stdout.strip(),flush=True)
 build=ROOT/'out/learning-checkpoints/54-game-adapter-check/root'
 result=run(['cmake','--build',str(build),'-j3']);(OUT/'root-build.log').write_text(result.stdout+result.stderr)
 result=run(['ctest','--test-dir',str(build),'--output-on-failure'],env=env);(OUT/'root-ctest.log').write_text(result.stdout);print(result.stdout[-300:],flush=True)
 assert run([str(build/'sim_hash_dump')]).stdout==(ROOT/'python/tests/_sim_hash_dump.txt').read_text()
 print('Character art identities, asset resolution, layout and cumulative regressions passed.',flush=True)
if __name__=='__main__':main()
