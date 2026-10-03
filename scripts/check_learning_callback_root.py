"""Instrument actual SDL backend: C++ heap operations and PCM retirement order."""
from pathlib import Path
import os,shlex
from check_learning_text_layout import run
from check_learning_utf8 import cut
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'out/learning-checkpoints/77-callback-check/root'
PRE=r'''
#include <mutex>
#include <new>
#include <cstdlib>
#include <cstdio>
#include <array>
#include <cstring>
static thread_local unsigned lock_depth=0,locked_new=0,locked_delete=0,callback_new=0,callback_delete=0,total_delete=0;
static thread_local bool callback_region=false;
void* operator new(std::size_t n){if(lock_depth)++locked_new;if(callback_region)++callback_new;if(void*p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void operator delete(void*p)noexcept{if(p){++total_delete;if(lock_depth)++locked_delete;if(callback_region)++callback_delete;}std::free(p);}
void operator delete(void*p,std::size_t)noexcept{::operator delete(p);}
struct TrackedLock{std::mutex& mu;explicit TrackedLock(std::mutex&m):mu(m){mu.lock();++lock_depth;}~TrackedLock(){--lock_depth;mu.unlock();}};
'''
POST=r'''
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"root callback line %d: %s\n",__LINE__,#x);std::exit(1);}}while(false)
int main(){
 CHECK(audio_init());SDL_PauseAudioDevice(s_dev,1);
 const auto h=audio_load_sound("Sounds/rotate.mp3");CHECK(h>0);
 const auto reference=s_sounds[h].pcm;CHECK(s_have.channels==2);
 audio_play_sound(h);std::size_t cursor=0;
 alignas(int16_t) std::array<unsigned char,2050> bytes;
 for(int i=0;i<40;++i){
  bytes.fill(0xCC);callback_region=true;audio_callback(nullptr,bytes.data(),2049);callback_region=false;
  const auto copied=std::min<std::size_t>(1024,reference.size()-cursor);
  CHECK(std::memcmp(bytes.data(),reference.data()+cursor,copied*2)==0);
  for(std::size_t j=copied*2;j<2049;++j)CHECK(bytes[j]==0);
  CHECK(bytes[2049]==0xCC);cursor+=copied;
 }
 CHECK(cursor==reference.size()&&callback_new==0&&callback_delete==0);
 locked_new=locked_delete=total_delete=0;
 audio_unload_sound(h);
 CHECK(total_delete>0&&locked_new==0);
 CHECK((locked_delete>0)==LEGACY);
 CHECK(!s_sounds[h].valid&&s_sounds[h].pcm.empty());
 callback_region=true;audio_callback(nullptr,bytes.data(),2049);callback_region=false;
 for(std::size_t j=0;j<2049;++j)CHECK(bytes[j]==0);
 CHECK(callback_new==0&&callback_delete==0&&lock_depth==0);
 audio_shutdown();
 std::puts(LEGACY?"Before: actual unload frees PCM under callback mutex.":"After: actual unload frees PCM after unlocking; callback exact samples/tail and C++ heap checks passed.");
}
'''
def main():
 OUT.mkdir(parents=True,exist_ok=True)
 src=(ROOT/'audio/sdl_audio.cpp').read_text();body=cut(src,'void audio_unload_sound(')
 before='''void audio_unload_sound(AudioHandle h) {
 if (!s_initialized || h <= 0 || h >= (int)s_sounds.size()) return;
 std::lock_guard<std::mutex> lk(s_mu);
 if(s_bgm.handle==h)s_bgm={}; if(s_currentMusic==h)s_currentMusic=0;
 for(auto&v:s_sfx)if(v.handle==h)v={};
 s_sounds[h].pcm.clear();s_sounds[h].pcm.shrink_to_fit();s_sounds[h].valid=false;
}'''
 flags=shlex.split(run(['pkg-config','--cflags','--libs','sdl2']).stdout)
 for label,text in [('before',src.replace(body,before)),('after',src)]:
  # Instrument only the lock guard, preserving its actual lock/unlock ordering.
  text=text.replace('std::lock_guard<std::mutex> lk(s_mu);','TrackedLock lk(s_mu);')
  p=OUT/(label+'.cpp');p.write_text(PRE+text+POST.replace('LEGACY','true' if label=='before' else 'false'));exe=OUT/label
  run(['c++','-std=c++17','-O1','-fsanitize=address,undefined','-fno-sanitize-recover=all','-I'+str(ROOT),'-I'+str(ROOT/'audio'),str(p),str(ROOT/'audio/mp3_decode.cpp'),*flags,'-pthread','-o',str(exe)])
  print(run([str(exe)],env={**os.environ,'SDL_AUDIODRIVER':'dummy'}).stdout.strip(),flush=True)
 print('No native Windows, hardware latency, C malloc or worst-case execution-time claim.',flush=True)
if __name__=='__main__':main()
