"""Real SDL pool plus actual Windows function bodies under an asynchronous API double."""
from pathlib import Path
import shlex
from check_learning_text_layout import run
from check_learning_utf8 import cut
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'out/learning-checkpoints/80-voice-pool-check/root-pool'
SDL=r'''
#define CHECK(e) do{if(!(e)){fprintf(stderr,"SDL pool %d: %s\n",__LINE__,#e);exit(1);}}while(false)
static void setup(bool short_last){
 s_initialized=true;s_have.channels=2;s_have.freq=44100;s_sfxEnabled=true;s_musicEnabled=true;
 s_sounds.clear();s_sounds.resize(11);s_bgm={};s_currentMusic=0;s_sfxOrder.reset();for(auto&v:s_sfx)v={};
 for(int i=1;i<11;++i){auto&d=s_sounds[i];d.pcm.assign(short_last&&i==8?2:32,100);d.channels=2;d.sampleRate=44100;d.valid=true;}
}
int main(){
 setup(false);for(int h=1;h<=8;++h)audio_play_sound(h);
 audio_play_sound(9);CHECK(s_sfx[0].handle==9);audio_play_sound(10);
 CHECK(s_sfx[BEFORE?0:1].handle==10);if(!BEFORE)CHECK(s_sfx[0].handle==9);
 setup(true);for(int h=1;h<=8;++h)audio_play_sound(h);
 int16_t out[2]{};audio_callback(nullptr,reinterpret_cast<Uint8*>(out),4);CHECK(out[0]==800);
 CHECK(s_sfx[7].active==BEFORE);audio_play_sound(9);CHECK(s_sfx[BEFORE?0:7].handle==9);
 setup(false);for(int i=0;i<8;++i)audio_play_sound(1);audio_play_music(1);
 audio_unload_sound(1);CHECK(!s_bgm.active&&s_currentMusic==0&&!s_sounds[1].valid&&s_sounds[1].pcm.empty());
 for(auto&v:s_sfx)CHECK(!v.active&&v.handle==0);
 audio_play_sound(1);audio_play_sound(-1);audio_play_sound(0);audio_play_sound(999);
 audio_callback(nullptr,reinterpret_cast<Uint8*>(out),4);CHECK(out[0]==0&&out[1]==0);
 puts(BEFORE?"Before SDL: repeated steal of slot0 and exact-end slot still active reproduced.":"After SDL: oldest starts rotate, exact-end reuse and all-reader unload passed.");
}
'''
WIN=r'''
#include "audio/voice_order.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <memory>
#include <algorithm>
using AudioHandle=int;using HRESULT=long;using UINT32=unsigned;
#define FAILED(h) ((h)<0)
#define SUCCEEDED(h) ((h)>=0)
constexpr int XAUDIO2_VOICE_NOSAMPLESPLAYED=0,XAUDIO2_END_OF_STREAM=1;
struct WAVEFORMATEX{unsigned nChannels=2,nSamplesPerSec=44100,wBitsPerSample=16;};
struct XAUDIO2_BUFFER{UINT32 AudioBytes=0;const uint8_t*pAudioData=nullptr;unsigned Flags=0;};
struct XAUDIO2_VOICE_STATE{unsigned BuffersQueued=0;};
static bool fail_create=false,fail_submit=false,fail_start=false;
struct MockVoice{
 bool alive=true;std::vector<const uint8_t*> readers;
 void GetState(XAUDIO2_VOICE_STATE*s,int){s->BuffersQueued=readers.size();}
 HRESULT Stop(){return 0;}
 // Model a legal delay: stop/flush have not retired the current read yet.
 HRESULT FlushSourceBuffers(){if(readers.size()>1)readers.resize(1);return 0;}
 void DestroyVoice(){alive=false;readers.clear();}
 HRESULT SetVolume(float){return 0;}
 HRESULT SubmitSourceBuffer(const XAUDIO2_BUFFER*b){if(fail_submit)return -2;readers.push_back(b->pAudioData);return 0;}
 HRESULT Start(){return fail_start?-3:0;}
};
static std::vector<std::unique_ptr<MockVoice>> all;
struct Engine{HRESULT CreateSourceVoice(MockVoice**v,const WAVEFORMATEX*){if(fail_create){*v=nullptr;return -1;}all.push_back(std::make_unique<MockVoice>());*v=all.back().get();return 0;}};
static Engine engine;static Engine*s_xaudio=&engine;
struct SoundData{std::vector<uint8_t>pcmData;WAVEFORMATEX format{};bool valid=true;};
static bool s_initialized=true,s_sfxEnabled=true;static float s_sfxVol=1;
static constexpr int MAX_SFX_VOICES=8;
static MockVoice*s_sfxVoices[8]{};static WAVEFORMATEX s_sfxFormats[8]{};static AudioHandle s_sfxHandles[8]{};
static audio_pool::VoiceOrder<8>s_sfxOrder;
static std::vector<SoundData>s_sounds(12);static int s_currentMusic=0,s_lastMusic=0;
static void audio_stop_music(){s_currentMusic=0;}
#define CHECK(e) do{if(!(e)){fprintf(stderr,"Windows contract %d: %s\n",__LINE__,#e);exit(1);}}while(false)
static bool reading(const uint8_t*p){for(auto&v:all)if(v->alive&&std::find(v->readers.begin(),v->readers.end(),p)!=v->readers.end())return true;return false;}
'''
WIN_POST=r'''
int main(){
 for(auto&d:s_sounds)d.pcmData.assign(20,1);
 audio_play_sound(1);for(int i=1;i<8;++i)audio_play_sound(2);
 const auto*old=s_sounds[1].pcmData.data();CHECK(reading(old));
 audio_play_sound(3);CHECK(s_sfxHandles[0]==3);
 CHECK(reading(old)==BEFORE); // old owner tracking was overwritten while a read may remain
 audio_unload_sound(1);
 audio_play_sound(4);CHECK(s_sfxHandles[BEFORE?0:1]==4);
 if(!BEFORE){
  fail_create=true;audio_play_sound(5);CHECK(!s_sfxVoices[2]&&s_sfxHandles[2]==0);fail_create=false;
  fail_submit=true;audio_play_sound(5);CHECK(!s_sfxVoices[2]&&s_sfxHandles[2]==0);fail_submit=false;
  fail_start=true;audio_play_sound(5);CHECK(!s_sfxVoices[2]&&s_sfxHandles[2]==0);CHECK(!reading(s_sounds[5].pcmData.data()));fail_start=false;
  audio_play_sound(5);CHECK(s_sfxHandles[2]==5);
  const auto*second=s_sounds[2].pcmData.data();audio_unload_sound(2);CHECK(!reading(second));
  audio_play_sound(-1);audio_play_sound(0);audio_play_sound(999);
 }
 puts(BEFORE?"Before Windows contract: delayed old PCM read can outlive overwritten handle; fixed-index steal reproduced.":"After Windows contract: DestroyVoice closes prior borrow; FIFO and create/submit/start failures passed.");
}
'''
def main():
 OUT.mkdir(parents=True,exist_ok=True)
 flags=shlex.split(run(['pkg-config','--cflags','--libs','sdl2']).stdout)
 src=(ROOT/'audio/sdl_audio.cpp').read_text();legacy=(ROOT/'tests/learning/legacy_sdl_pool.inc').read_text();before=src
 for sym in ['static void mix_voice(','void audio_play_sound(']:before=before.replace(cut(before,sym),cut(legacy,sym))
 for name,body in [('before',before),('after',src)]:
  p=OUT/('sdl-'+name+'.cpp');p.write_text(body+SDL.replace('BEFORE','true'if name=='before'else'false'));exe=p.with_suffix('')
  run(['c++','-std=c++17','-O1','-fsanitize=address,undefined','-fno-sanitize-recover=all','-I'+str(ROOT),'-I'+str(ROOT/'audio'),str(p),str(ROOT/'audio/mp3_decode.cpp'),*flags,'-pthread','-o',str(exe)])
  print(run([str(exe)]).stdout.strip(),flush=True)
 win=(ROOT/'audio/audio.cpp').read_text()
 common=cut(win,'static bool FormatMatches(')+cut(win,'void audio_unload_sound(')
 legacy=(ROOT/'tests/learning/legacy_windows_pool.inc').read_text()
 for name,body in [('before',cut(legacy,'void audio_play_sound(')),('after',cut(win,'void audio_play_sound('))]:
  p=OUT/('windows-'+name+'.cpp');p.write_text(WIN+common+body+WIN_POST.replace('BEFORE','true'if name=='before'else'false'));exe=p.with_suffix('')
  run(['c++','-std=c++17','-O1','-fsanitize=address,undefined','-fno-sanitize-recover=all','-I'+str(ROOT),str(p),'-o',str(exe)])
  print(run([str(exe)]).stdout.strip(),flush=True)
 print('Windows native ABI/hardware not executed; asynchronous contract schedule is a test double.',flush=True)
if __name__=='__main__':main()
