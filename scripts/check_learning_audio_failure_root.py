"""Lesson 82: actual Game isolation and bounded Windows playback diagnostics."""
from pathlib import Path
import subprocess
from check_learning_text_layout import run
from check_learning_utf8 import cut
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'out/learning-checkpoints/82-audio-failure-check/root'
def main():
 OUT.mkdir(parents=True,exist_ok=True)
 source=(ROOT/'audio/audio.cpp').read_text();legacy=(ROOT/'tests/learning/legacy_audio_failure_logs.inc').read_text()
 before=source
 for symbol in ['void audio_play_sound(','static void start_music_voice(']:before=before.replace(cut(before,symbol),cut(legacy,symbol))
 (OUT/'before.cpp').write_text(before)
 body=r'''
#include "BACKEND"
#include <cstdio>
#include <cstdlib>
#define CHECK(e) do{if(!(e)){std::fprintf(stderr,"diag %d: %s\n",__LINE__,#e);std::exit(1);}}while(false)
namespace f=fake_xaudio;
int main(int argc,char**argv){CHECK(argc==2);
 for(int lifetime=0;lifetime<2;++lifetime){
  f::reset();CHECK(audio_init());auto h=audio_load_sound(argv[1]);CHECK(h);
  for(auto stage:{f::Fail::source,f::Fail::submit,f::Fail::start}){
   f::fail=stage;
   for(int i=0;i<100;++i){audio_play_sound(h);audio_play_music(h);}
   CHECK(f::sources==0);f::read_all();
   if(stage==f::Fail::source)CHECK(audio_init()); // Shared user must not reset notices.
  }
  f::fail=f::Fail::none;audio_play_sound(h);audio_play_music(h);CHECK(f::sources==2);
  audio_unload_sound(h);CHECK(f::sources==0);audio_shutdown();CHECK(f::engines==1);audio_shutdown();
  CHECK(f::engines==0&&f::com_refs==0);
 }
 std::puts("Real Windows backend: failure diagnostics bounded per device lifetime, shared init preserves gate, future play and cleanup unaffected (API double).");
}
'''
 flags=['c++','-std=c++17','-O1','-fsanitize=address,undefined','-fno-sanitize-recover=all','-I'+str(ROOT/'docs/learn/checkpoints/82-audio-failure/tests/xaudio_fake'),'-I'+str(ROOT/'audio'),'-I'+str(ROOT)]
 for version,cpp in [('before',OUT/'before.cpp'),('after',ROOT/'audio/audio.cpp')]:
  file=OUT/(version+'-test.cpp');file.write_text(body.replace('BACKEND',str(cpp)));binary=OUT/version
  run([*flags,str(file),str(ROOT/'audio/mp3_decode.cpp'),'-o',str(binary)])
  r=run([str(binary),str(ROOT/'Sounds/rotate.mp3')]);(OUT/(version+'.log')).write_text(r.stdout+r.stderr)
  messages=['CreateSourceVoice failed','SFX submit/start failed','CreateSourceVoice (music) failed','Music submit/start failed']
  expected=[200,400,200,400] if version=='before' else [2,2,2,2]
  assert [r.stderr.count(m) for m in messages]==expected
  print(version+': '+str(sum(expected))+' diagnostic lines for 1200 failed requests across two lifetimes',flush=True)
 binary=OUT/'game-isolation'
 run(['c++','-std=c++17','-O1','-g','-fsanitize=address,undefined','-fno-sanitize-recover=all','-I'+str(ROOT),'-I'+str(ROOT/'src'),*[str(ROOT/f) for f in ['tests/game_wrapper_test.cpp','src/game.cpp','src/colors.cpp','src/sim_game.cpp','src/position.cpp']],'-o',str(binary)])
 r=run([str(binary)]);(OUT/'game-isolation.log').write_text(r.stdout+r.stderr);print(r.stdout.strip(),flush=True)
 print('Actual Game: init/load/play failures, 17280 matched ticks and consumed sound flags passed.',flush=True)
if __name__=='__main__':main()
