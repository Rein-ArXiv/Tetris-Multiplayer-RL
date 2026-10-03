"""Root lifecycle regressions: real SDL ownership and API-contract Windows doubles."""
from pathlib import Path
import os,shlex
from check_learning_text_layout import run
from check_learning_utf8 import cut
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'out/learning-checkpoints/76-playback-check/root'
PRE=r'''
#include <SDL.h>
#include <cstdio>
#include <cstdlib>
#include <new>
static bool fail_new=false,fail_open=false;static int open_calls=0;
void* operator new(std::size_t n){if(fail_new){fail_new=false;throw std::bad_alloc();}if(auto*p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void operator delete(void*p)noexcept{std::free(p);}void operator delete(void*p,std::size_t)noexcept{std::free(p);}
static SDL_AudioDeviceID open_device(const char*a,int b,const SDL_AudioSpec*c,SDL_AudioSpec*d,int e){++open_calls;if(fail_open)return 0;return SDL_OpenAudioDevice(a,b,c,d,e);}
#define SDL_OpenAudioDevice open_device
'''
POST=r'''
#define CHECK(e) do{if(!(e)){fprintf(stderr,"root lifecycle %d: %s\n",__LINE__,#e);exit(1);}}while(false)
int main(){
 CHECK(SDL_InitSubSystem(SDL_INIT_AUDIO)==0);fail_open=true;
 CHECK(!audio_init());CHECK(SDL_WasInit(SDL_INIT_AUDIO)&SDL_INIT_AUDIO);audio_shutdown();
 CHECK(bool(SDL_WasInit(SDL_INIT_AUDIO)&SDL_INIT_AUDIO)==!BEFORE);
 SDL_QuitSubSystem(SDL_INIT_AUDIO);fail_open=false;
 std::vector<SoundData>().swap(s_sounds);open_calls=0;fail_new=true;bool threw=false,ok=false;
 try{ok=audio_init();}catch(const std::bad_alloc&){threw=true;}fail_new=false;
 CHECK(!ok&&threw==BEFORE&&open_calls==(BEFORE?1:0));
 if(BEFORE)CHECK(s_dev!=0);else CHECK(s_dev==0);
 audio_shutdown();CHECK(!(SDL_WasInit(SDL_INIT_AUDIO)&SDL_INIT_AUDIO));
 CHECK(audio_init());auto device=s_dev;CHECK(audio_init());audio_shutdown();CHECK(s_dev==device&&s_initialized);
 SDL_PauseAudioDevice(s_dev,1);auto clip=audio_load_sound("Sounds/rotate.mp3");CHECK(clip>0);audio_play_music(clip);s_bgm.pos=2;
 audio_set_music_enabled(true);CHECK(s_bgm.pos==(BEFORE?0:2));audio_unload_sound(clip);
 s_currentMusic=123;audio_shutdown();CHECK(s_currentMusic==(BEFORE?123:0));CHECK(s_dev==0&&!s_initialized);
 audio_shutdown();CHECK(s_refCount==0);
 printf("%s SDL: failed-open reference, allocation boundary, shared init and stale music handle checks passed.\n",BEFORE?"Before reproducer":"After");
}
'''
WIN=r'''
#include <vector>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
struct XAUDIO2_VOICE_STATE{unsigned BuffersQueued=1;};constexpr int XAUDIO2_VOICE_NOSAMPLESPLAYED=0;
using AudioHandle=int;static bool s_initialized=true;static int s_currentMusic=0,s_lastMusic=0;
struct Format{int marker=9;};
struct SoundData{std::vector<uint8_t> pcmData;bool valid;};static std::vector<SoundData> s_sounds={{{},false},{{1,2,3,4},true}};
static bool read_still_possible=true;static int sleeps=0,destroys=0;
void Sleep(int){++sleeps;}
struct MockVoice{void Stop(){}void FlushSourceBuffers(){}void GetState(XAUDIO2_VOICE_STATE*s,int){s->BuffersQueued=1;}void DestroyVoice(){if(s_sounds[1].pcmData.empty())std::abort();read_still_possible=false;++destroys;}};
static constexpr int MAX_SFX_VOICES=1;static MockVoice mock;
static MockVoice* s_sfxVoices[]={&mock};static Format s_sfxFormats[1];static int s_sfxHandles[]={1};
void audio_stop_music(){s_currentMusic=0;s_lastMusic=0;}
'''
def main():
 OUT.mkdir(parents=True,exist_ok=True);flags=shlex.split(run(['pkg-config','--cflags','--libs','sdl2']).stdout)
 after=(ROOT/'audio/sdl_audio.cpp').read_text()
 # Reconstruct only the immediately preceding lifecycle logic as a fixture.
 before=after.replace('    if (s_musicEnabled == on) return; // Setting a state is not a replay command.\n','').replace('    s_audioOwned = true;\n','').replace('        s_audioOwned = false;\n','')
 a=before.index('    // Prepare C++ storage before acquiring OS resources.');b=before.index('    if (SDL_InitSubSystem',a);before=before[:a]+before[b:]
 a=before.index('    for (auto& v : s_sfx)',before.index('bool audio_init()'));before=before[:a]+'    s_sounds.clear();\n    s_sounds.push_back(SoundData{});\n'+before[a:]
 a=before.index('    if (s_audioOwned)',before.index('void audio_shutdown()'));b=before.index('    s_sounds.clear();',a);before=before[:a]+'    SDL_QuitSubSystem(SDL_INIT_AUDIO);\n'+before[b:]
 for label,src in [('before',before),('after',after)]:
  p=OUT/(label+'.cpp');p.write_text(PRE+src+POST.replace('BEFORE','true' if label=='before' else 'false'));exe=OUT/label
  run(['c++','-std=c++17','-O1','-fsanitize=address,undefined','-I'+str(ROOT),'-I'+str(ROOT/'audio'),str(p),str(ROOT/'audio/mp3_decode.cpp'),*flags,'-pthread','-o',str(exe)])
  print(run([str(exe)],env={**os.environ,'SDL_AUDIODRIVER':'dummy'}).stdout.strip(),flush=True)
 body=cut((ROOT/'audio/audio.cpp').read_text(),'void audio_unload_sound(')
 old=body.replace('''        // DestroyVoice waits until this voice can no longer read the PCM.
        // A bounded polling timeout is not proof that the buffer is unused.
        s_sfxVoices[i]->DestroyVoice();
        s_sfxVoices[i] = nullptr;
        s_sfxFormats[i] = {};
''','''        s_sfxVoices[i]->Stop();
        s_sfxVoices[i]->FlushSourceBuffers();
        for (int spin=0;spin<100;++spin) { XAUDIO2_VOICE_STATE st; s_sfxVoices[i]->GetState(&st,XAUDIO2_VOICE_NOSAMPLESPLAYED); if(st.BuffersQueued==0)break; Sleep(1); }
''')
 for label,fn in [('before',old),('after',body)]:
  p=OUT/('windows-'+label+'.cpp');p.write_text(WIN+fn+'''\nint main(){audio_unload_sound(1);if(!s_sounds[1].pcmData.empty()||s_sounds[1].valid)return 1;if(read_still_possible!=BEFORE)return 2;if(destroys!=(BEFORE?0:1)||sleeps!=(BEFORE?100:0))return 3;std::puts("Windows unload body: buffer lifetime ordering checked (contract double).");}'''.replace('BEFORE','true' if label=='before' else 'false'));exe=OUT/('windows-'+label);run(['c++','-std=c++17','-O1','-fsanitize=address,undefined',str(p),'-o',str(exe)]);print(run([str(exe)]).stdout.strip(),flush=True)
 print('Native Windows/XAudio2 not run; DestroyVoice guarantee checked against Microsoft API contract.',flush=True)
if __name__=='__main__':main()
