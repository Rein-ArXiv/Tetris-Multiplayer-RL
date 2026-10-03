"""Exercise real CPU loader and SDL backend; no native Windows playback claim."""
from pathlib import Path
import os,shlex
from check_learning_text_layout import run
from check_learning_utf8 import cut
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'out/learning-checkpoints/75-mp3-check/root-mp3'
SOURCE=r'''
#include <SDL.h>
#include <cstdlib>
#include <cstdio>
#include <new>
#include <dirent.h>
static int fail_at=-1,calls=0,mode=0,streams=0;
void* operator new(std::size_t n){if(fail_at>=0&&calls++==fail_at){fail_at=-1;throw std::bad_alloc();}if(void*p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void operator delete(void*p) noexcept{std::free(p);}
void operator delete(void*p,std::size_t) noexcept{std::free(p);}
static SDL_AudioStream* new_stream(SDL_AudioFormat s,Uint8 c,int r,SDL_AudioFormat d,Uint8 dc,int dr){auto* p=SDL_NewAudioStream(s,c,r,d,dc,dr);if(p)++streams;return p;}
static void free_stream(SDL_AudioStream*p){if(p)--streams;SDL_FreeAudioStream(p);}
static int available(SDL_AudioStream*p){const int n=SDL_AudioStreamAvailable(p);return mode==3?n+1:n;}
static int get(SDL_AudioStream*p,void*out,int n){if(mode==1)return -1;int got=SDL_AudioStreamGet(p,out,n);return mode==2?got-4:got;}
#define SDL_NewAudioStream new_stream
#define SDL_FreeAudioStream free_stream
#define SDL_AudioStreamAvailable available
#define SDL_AudioStreamGet get
#include "audio/sdl_audio.cpp"
#define CHECK(e) do{if(!(e)){std::fprintf(stderr,"MP3 root line %d: %s\n",__LINE__,#e);std::exit(1);}}while(false)
static int descriptors(){DIR*d=opendir("/proc/self/fd");CHECK(d);int n=0;while(readdir(d))++n;closedir(d);return n;}
int main(){
 CHECK(audio_init());SDL_PauseAudioDevice(s_dev,1);
 const char* path="Sounds/rotate.mp3";
 int h=audio_load_sound(path);CHECK(h>0&&s_sounds[h].valid&&streams==0);audio_unload_sound(h);
 const auto baseline=s_sounds.size();
 for(int m:{1,2,3}){mode=m;CHECK(audio_load_sound(path)==0&&s_sounds.size()==baseline&&streams==0);}mode=0;
 const int fd_before=descriptors();int rejected=0,accepted=0;
 for(int i=0;i<16;++i){const auto size=s_sounds.size();calls=0;fail_at=i;h=audio_load_sound(path);fail_at=-1;if(h){++accepted;audio_unload_sound(h);}else{++rejected;CHECK(s_sounds.size()==size);}CHECK(streams==0&&descriptors()==fd_before);}
 CHECK(rejected>=3&&accepted>0);
 CHECK(audio_load_sound(nullptr)==0&&audio_load_sound("")==0);
 for(const char* asset:{"Sounds/rotate.mp3","Sounds/clear.mp3","Sounds/music.mp3"}) {
  auto decoded=audio_mp3::load(asset);CHECK(decoded&&decoded.samples.size()*2<=64u*1024u*1024u);
  h=audio_load_sound(asset);CHECK(h>0&&s_sounds[h].valid);audio_unload_sound(h);
 }
 audio_shutdown();CHECK(streams==0);
 std::printf("Real SDL: %d allocation failures, %d successful injections; descriptors/streams cleaned; 3 real MP3 assets loaded.\n",rejected,accepted);
}
'''
# File wrapper fault injection is isolated from dr_mp3 implementation's own stdio calls.
FILE_FAULT=r'''
#include <cstdio>
#include <cstdlib>
static int mode=0,open_files=0;
static FILE* open_file(const char*p,const char*m){auto*f=std::fopen(p,m);if(f)++open_files;return f;}
static int close_file(FILE*f){--open_files;return std::fclose(f);}
static int seek_file(FILE*f,long off,int origin){return mode==1?-1:std::fseek(f,off,origin);}
static size_t read_file(void*p,size_t s,size_t n,FILE*f){if(mode==2)return std::fread(p,s,n? n-1:0,f);return std::fread(p,s,n,f);}
static int extra_file(FILE*f){if(mode==3)return 42;return std::fgetc(f);}
'''
def main():
 OUT.mkdir(parents=True,exist_ok=True)
 flags=shlex.split(run(['pkg-config','--cflags','--libs','sdl2']).stdout)
 p=OUT/'real-faults.cpp';p.write_text(SOURCE);exe=OUT/'real-faults'
 run(['c++','-std=c++17','-O1','-fsanitize=address,undefined','-fno-sanitize-recover=all','-I'+str(ROOT),str(p),str(ROOT/'audio/mp3_decode.cpp'),*flags,'-pthread','-o',str(exe)])
 result=run([str(exe)],env={**os.environ,'SDL_AUDIODRIVER':'dummy'});print(result.stdout.strip(),flush=True)
 # Transform only the stdio call sites in the real wrapper under test.
 impl=(ROOT/'audio/mp3_decode.cpp').read_text().replace('#include "../third_party/dr_mp3.h"','#include "third_party/dr_mp3.h"').replace('#include "mp3_decode.h"','#include "audio/mp3_decode.h"').replace('#include "pcm_layout.h"','#include "audio/pcm_layout.h"')
 for a,b in [('std::fopen','open_file'),('std::fclose','close_file'),('std::fseek','seek_file'),('std::fread','read_file'),('std::fgetc','extra_file')]:impl=impl.replace(a,b)
 p=OUT/'file-faults.cpp';p.write_text(FILE_FAULT+impl+r'''
int main(){for(int m:{1,2,3}){mode=m;auto r=audio_mp3::load("Sounds/rotate.mp3");if(r||r.error!=audio_mp3::Error::io||open_files!=0)return 1;}mode=0;auto good=audio_mp3::load("Sounds/rotate.mp3");if(!good||open_files!=0)return 2;std::puts("Actual file wrapper: seek, short read and post-read growth faults rejected; FILE closed.");}
''');exe=OUT/'file-faults';run(['c++','-std=c++17','-O1','-fsanitize=address,undefined','-I'+str(ROOT),str(p),'-o',str(exe)]);print(run([str(exe)]).stdout.strip(),flush=True)
 # Compile the production Windows loader body with surrogate API data types.
 # This checks portable C++ logic, not SDK ABI, DLL loading or device playback.
 win=(ROOT/'audio/audio.cpp').read_text()
 pre=r'''#include "audio/mp3_decode.h"
#include "audio/pcm_layout.h"
#include <vector>
#include <cstdio>
#include <cstring>
#include <limits>
#include <new>
#include <stdexcept>
using WORD=uint16_t;using DWORD=uint32_t;using AudioHandle=int;
struct WAVEFORMATEX{WORD wFormatTag,nChannels;DWORD nSamplesPerSec,nAvgBytesPerSec;WORD nBlockAlign,wBitsPerSample,cbSize;};
constexpr WORD WAVE_FORMAT_PCM=1;
static size_t XAUDIO2_MAX_BUFFER_BYTES=1024; // deliberately small surrogate cap
struct SoundData{std::vector<uint8_t> pcmData;WAVEFORMATEX format{};bool valid=false;};
static std::vector<SoundData> s_sounds(1);static bool s_initialized=true;
'''
 p=OUT/'windows-body.cpp';p.write_text(pre+cut(win,'static WAVEFORMATEX MakeWaveFormat(')+cut(win,'AudioHandle audio_load_sound(')+r'''
int main(){if(audio_load_sound("Sounds/rotate.mp3")!=0||s_sounds.size()!=1)return 1;
XAUDIO2_MAX_BUFFER_BYTES=64u*1024u*1024u;
for(const char* path:{"Sounds/rotate.mp3","Sounds/clear.mp3","Sounds/music.mp3"}){
auto h=audio_load_sound(path);if(h<=0)return 2;const auto& x=s_sounds[h];auto d=audio_mp3::load(path);
if(!d||!x.valid||x.pcmData.size()!=d.samples.size()*2||x.format.nChannels!=d.channels||x.format.nSamplesPerSec!=d.rate||x.format.nBlockAlign!=d.channels*2||x.format.nAvgBytesPerSec!=d.rate*d.channels*2||std::memcmp(x.pcmData.data(),d.samples.data(),x.pcmData.size()))return 3;}
if(audio_load_sound(nullptr)||audio_load_sound(""))return 4;
std::puts("Windows production loader body: 3 real assets, exact PCM copy/format and surrogate cap passed (not native Windows).");}
''')
 exe=OUT/'windows-body';run(['c++','-std=c++17','-O1','-fsanitize=address,undefined','-I'+str(ROOT),str(p),str(ROOT/'audio/mp3_decode.cpp'),'-o',str(exe)]);print(run([str(exe)]).stdout.strip(),flush=True)

if __name__=='__main__':main()
