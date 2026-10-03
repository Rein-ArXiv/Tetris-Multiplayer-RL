"""Compile the complete production Windows backend against a behavioral double."""
from pathlib import Path
import subprocess
from check_learning_text_layout import run
from check_learning_utf8 import cut
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'out/learning-checkpoints/81-xaudio2-check/root-xaudio'
SRC=ROOT/'audio/audio.cpp'
def main():
 OUT.mkdir(parents=True,exist_ok=True)
 source=SRC.read_text()
 # Keep only the historical BGM behavior different for the before oracle.
 before=source.replace(cut(source,'static void start_music_voice('), (ROOT/'tests/learning/legacy_xaudio_music.inc').read_text().rstrip())
 (OUT/'before.cpp').write_text(before)
 test=r'''
#include "BACKEND"
#include <cassert>
#define CHECK(e) do{if(!(e)){std::fprintf(stderr,"root XAudio %d: %s\n",__LINE__,#e);std::exit(1);}}while(false)
namespace f=fake_xaudio;
int main(int argc,char**argv){
 CHECK(argc==2);
 for(auto stage:{f::Fail::com,f::Fail::engine,f::Fail::master}){
  f::reset();f::fail=stage;CHECK(!audio_init());audio_shutdown();
  CHECK(f::com_refs==0&&f::sources==0&&f::masters==0&&f::engines==0);
 }
 for(auto stage:{f::Fail::source,f::Fail::submit,f::Fail::start}){
  f::reset();CHECK(audio_init());CHECK(audio_init());CHECK(f::inits==1);
  const auto h=audio_load_sound(argv[1]);CHECK(h>0);
  f::fail=stage;audio_play_music(h);
#ifdef BEFORE
  if(stage!=f::Fail::source){CHECK(s_musicVoice!=nullptr&&s_currentMusic==h);}
#else
  CHECK(s_musicVoice==nullptr&&s_currentMusic==0&&f::sources==0);
#endif
  CHECK(s_lastMusic==h);f::read_all();
  f::fail=f::Fail::none;audio_set_music_enabled(false);audio_set_music_enabled(true);
  CHECK(s_musicVoice&&s_currentMusic==h);f::read_all();
  for(int i=0;i<10;++i)audio_play_sound(h);
  CHECK(f::sources==9);audio_unload_sound(h);f::read_all();CHECK(f::sources==0);
  audio_shutdown();CHECK(f::engines==1);audio_shutdown();
  CHECK(f::com_refs==0&&f::engines==0&&f::masters==0);
 }
 for(auto result:{S_OK,S_FALSE,RPC_E_CHANGED_MODE}){
  f::reset();f::com_result=result;f::com_refs=result==S_OK?0:2;const auto host=f::com_refs;
  CHECK(audio_init());audio_shutdown();CHECK(f::com_refs==host);
 }
 std::puts("Whole production XAudio backend: BGM failure state, retry, PCM release, SFX pool, shared init/COM cleanup passed (API double).");
}
'''
 flags=['c++','-std=c++17','-O1','-fsanitize=address,undefined','-fno-sanitize-recover=all','-I'+str(ROOT/'docs/learn/checkpoints/81-xaudio2/tests/xaudio_fake'),'-I'+str(ROOT/'audio'),'-I'+str(ROOT)]
 for version,path in [('before',OUT/'before.cpp'),('after',SRC)]:
  testfile=OUT/(version+'-test.cpp');testfile.write_text(test.replace('BACKEND',str(path)))
  binary=OUT/version
  run([*flags,*(['-DBEFORE'] if version=='before' else []),str(testfile),str(ROOT/'audio/mp3_decode.cpp'),'-o',str(binary)])
  r=run([str(binary),str(ROOT/'Sounds/rotate.mp3')]);(OUT/(version+'.log')).write_text(r.stdout+r.stderr);print(version+': '+r.stdout.strip(),flush=True)
 # The SDK's function-like max macro collides even with a qualified max().
 no_guard=source.replace('#ifndef NOMINMAX\n#define NOMINMAX\n#endif\n','',1)
 (OUT/'no-guard.cpp').write_text(no_guard)
 r=subprocess.run([*flags,'-c',str(OUT/'no-guard.cpp'),'-o',str(OUT/'no-guard.o')],capture_output=True,text=True)
 assert r.returncode!=0 and 'max' in r.stderr and 'requires 2 arguments' in r.stderr,r.stderr
 (OUT/'no-guard.log').write_text(r.stderr)
 print('Missing NOMINMAX compile failure reproduced; guarded whole backend compiled.',flush=True)
if __name__=='__main__':main()
