"""Actual loader bodies with controlled decoder/SDL boundaries, plus native SDL smoke."""
from pathlib import Path
import os,shlex
from check_learning_text_layout import run
from check_learning_utf8 import cut
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'out/learning-checkpoints/74-pcm-check/root-pcm'
PRE=r'''
#include <SDL.h>
#include "audio/pcm_layout.h"
#include <vector>
#include <limits>
#include <mutex>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <climits>
#define CHECK(e) do{if(!(e)){std::fprintf(stderr,"root PCM %d: %s\n",__LINE__,#e);std::exit(1);}}while(false)
using AudioHandle=int;using drmp3_uint64=uint64_t;using drmp3_uint32=uint32_t;using drmp3_int16=int16_t;
struct drmp3_config{uint32_t channels,sampleRate;};
static uint64_t count=32;static uint32_t channels=1,rate=8000;static int mode=0;static int live=0;
static drmp3_int16* drmp3_open_memory_and_read_pcm_frames_s16(const void*,size_t,drmp3_config* cfg,uint64_t* frames,void*){
 *cfg={channels,rate};*frames=count;++live;
 auto p=static_cast<int16_t*>(calloc(count==UINT64_MAX?1:count*channels,sizeof(int16_t)));CHECK(p);return p;
}
static void drmp3_free(void* p,void*){--live;free(p);}
static bool s_initialized=true;
'''
SDL=r'''
struct SoundData{std::vector<int16_t> pcm;uint32_t channels=0,sampleRate=0;bool valid=false;};
static std::vector<SoundData> s_sounds(1);static std::mutex s_mu;
static SDL_AudioSpec s_have=[](){SDL_AudioSpec s{};s.channels=2;s.freq=44100;return s;}();
static int available(SDL_AudioStream* c){int n=SDL_AudioStreamAvailable(c);return mode==3?n+1:n;}
static int get(SDL_AudioStream* c,void* data,int len){if(mode==1)return -1;int n=SDL_AudioStreamGet(c,data,len);return mode==2?n-4:n;}
#define SDL_AudioStreamAvailable available
#define SDL_AudioStreamGet get
'''
WIN=r'''
using WORD=uint16_t;using DWORD=uint32_t;
struct WAVEFORMATEX{WORD wFormatTag,nChannels;DWORD nSamplesPerSec,nAvgBytesPerSec;WORD nBlockAlign,wBitsPerSample,cbSize;};
constexpr WORD WAVE_FORMAT_PCM=1;
// Deliberately small surrogate API cap to test the production branch without GB allocations.
constexpr size_t XAUDIO2_MAX_BUFFER_BYTES=1024;
struct SoundData{std::vector<uint8_t> pcmData;WAVEFORMATEX format{};bool valid=false;};
static std::vector<SoundData> s_sounds(1);
'''
def main():
 if '#include "mp3_decode.h"' in (ROOT/'audio/sdl_audio.cpp').read_text():
  print(run(['python3',str(ROOT/'scripts/check_learning_mp3_root.py')]).stdout,flush=True)
  return
 OUT.mkdir(parents=True,exist_ok=True)
 flags=shlex.split(run(['pkg-config','--cflags','--libs','sdl2']).stdout)
 # Before/after is a compact checked fixture, not a dependency on an untracked backup.
 current=(ROOT/'audio/sdl_audio.cpp').read_text();new=cut(current,'AudioHandle audio_load_sound(')
 old=new.replace('const int got = SDL_AudioStreamGet(conv, sd.pcm.data(), outBytes);','SDL_AudioStreamGet(conv, sd.pcm.data(), outBytes);\n        const int got = outBytes;')
 for name,body in [('before-get',old),('after',new)]:
  src=PRE+SDL+body+r'''
int main(int argc,char**argv){CHECK(argc==2);const char* file=argv[1];
 auto h=audio_load_sound(file);CHECK(h>0&&s_sounds[h].valid&&live==0);
 CHECK(s_sounds[h].channels==2&&s_sounds[h].sampleRate==44100&&s_sounds[h].pcm.size()%2==0);
 for(int m:{1,2}){mode=m;auto result=audio_load_sound(file);CHECK(EXPECT_BAD?result>0:result==0);CHECK(live==0);}
 mode=3;CHECK(audio_load_sound(file)==0&&live==0);mode=0;
 count=UINT64_MAX;CHECK(audio_load_sound(file)==0&&live==0);count=32;
 channels=2;rate=44100;CHECK(audio_load_sound(file)>0&&live==0);
 CHECK(audio_load_sound(nullptr)==0&&audio_load_sound("")==0);
 std::puts("SDL loader: native resampling, failure/short read/alignment, extreme size and direct-copy paths passed.");}
'''
  p=OUT/(name+'.cpp');p.write_text(src.replace('EXPECT_BAD','true' if name=='before-get' else 'false'))
  exe=OUT/name;run(['c++','-std=c++17','-O1','-fsanitize=undefined','-fno-sanitize-recover=all','-I'+str(ROOT),str(p),*flags,'-o',str(exe)])
  print(run([str(exe),str(ROOT/'Sounds/rotate.mp3')]).stdout.strip(),flush=True)
 win=(ROOT/'audio/audio.cpp').read_text()
 src=PRE+WIN+cut(win,'static WAVEFORMATEX MakeWaveFormat(')+cut(win,'AudioHandle audio_load_sound(')+r'''
int main(int argc,char**argv){CHECK(argc==2);count=128;channels=2;rate=44100;
 auto h=audio_load_sound(argv[1]);CHECK(h>0&&s_sounds[h].pcmData.size()==512&&s_sounds[h].format.nBlockAlign==4&&s_sounds[h].format.nAvgBytesPerSec==176400&&live==0);
 count=257;CHECK(audio_load_sound(argv[1])==0&&live==0);
 count=UINT64_MAX;CHECK(audio_load_sound(argv[1])==0&&live==0);
 CHECK(audio_load_sound(nullptr)==0);std::puts("XAudio loader body: format and surrogate buffer-cap rejection passed (not a Windows native run).");}
'''
 p=OUT/'win-body.cpp';p.write_text(src);exe=OUT/'win-body';run(['c++','-std=c++17','-O1','-fsanitize=undefined','-I'+str(ROOT),str(p),*flags,'-o',str(exe)]);print(run([str(exe),str(ROOT/'Sounds/rotate.mp3')]).stdout.strip(),flush=True)
 # The complete real SDL implementation, real bundled MP3, dummy device.
 p=OUT/'real.cpp';p.write_text('#include "audio/sdl_audio.cpp"\n#include <cstdlib>\nint main(){if(!audio_init())return 1;auto h=audio_load_sound("Sounds/rotate.mp3");if(!h)return 2;audio_play_sound(h);SDL_Delay(30);audio_unload_sound(h);audio_shutdown();return 0;}\n')
 exe=OUT/'real';run(['c++','-std=c++17','-O1','-I'+str(ROOT),str(p),*flags,'-pthread','-o',str(exe)])
 run([str(exe)],env={**os.environ,'SDL_AUDIODRIVER':'dummy'});print('Full SDL backend + real MP3 + dummy device smoke passed.',flush=True)
if __name__=='__main__':main()
