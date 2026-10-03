#include "audio/voice_pool.h"
#include <algorithm>
#include <deque>
#include <cstdio>
#include <cstdlib>
#include <vector>
#define CHECK(e) do{if(!(e)){std::fprintf(stderr,"pool line %d: %s\n",__LINE__,#e);std::exit(1);}}while(false)
using study_audio::VoicePool;
using study_audio::Pcm16;
struct Expected {int owner=-1;std::size_t cursor=0;bool active=false;};
int main(){
 audio_pool::VoiceOrder<8> order;
 for(std::size_t i=0;i<8;++i)CHECK(order.mark_started(i));
 CHECK(order.oldest()==0);CHECK(order.mark_started(0)&&order.oldest()==1);
 CHECK(order.mark_started(1)&&order.oldest()==2);
 const auto old=order.ordered();CHECK(!order.mark_started(8)&&old==order.ordered());
 VoicePool pool;CHECK(!pool.playing(0));CHECK(pool.configure(44100,2));
 const std::array<int,4> values{101,-211,333,444};
 const std::array<std::size_t,4> lengths{1,2,4,8};
 std::array<std::optional<Pcm16>,4> clips;
 for(std::size_t i=0;i<4;++i)clips[i]=Pcm16::make(44100,2,std::vector<std::int16_t>(lengths[i]*2,values[i]));
 auto wrong=Pcm16::make(48000,2,std::vector<std::int16_t>(4,9));CHECK(wrong);
 std::array<Expected,audio_mix::kMaxVoices> ref{};
 std::deque<std::size_t> starts;
 unsigned rng=37;
 for(unsigned step=0;step<5000;++step){
  rng=rng*1664525u+1013904223u;const unsigned action=(rng>>24)%10;const unsigned owner=(rng>>16)%4;
  if(action<6){
   // Rejection must not consume a slot or change its age.
   CHECK(!pool.start(VoicePool::kNoOwner,*clips[owner]));CHECK(!pool.start(owner,*wrong));CHECK(!pool.configure(0,2));
   std::size_t slot=ref.size();
   for(std::size_t i=0;i<ref.size();++i)if(!ref[i].active){slot=i;break;}
   if(slot==ref.size()){CHECK(!starts.empty());slot=starts.front();}
   starts.erase(std::remove(starts.begin(),starts.end(),slot),starts.end());starts.push_back(slot);
   CHECK(pool.start(owner,*clips[owner])==slot);
   ref[slot]={static_cast<int>(owner),0,true};
  }else if(action==6){
   pool.stop_owner(owner);
   for(auto&v:ref)if(v.owner==static_cast<int>(owner))v={};
  }else{
   const std::size_t frames=(rng>>8)%8;
   std::vector<std::int16_t> out(frames*2,0x1234);pool.render(out.data(),out.size()*2);
   for(std::size_t f=0;f<frames;++f){
    int sum=0;
    for(auto&v:ref)if(v.active){sum+=values[v.owner];++v.cursor;if(v.cursor==lengths[v.owner])v.active=false;}
    CHECK(out[f*2]==sum&&out[f*2+1]==sum);
   }
  }
  for(std::size_t i=0;i<ref.size();++i){CHECK(pool.voice_playing(i)==ref[i].active);CHECK(pool.owner_of(i)==(ref[i].owner<0?std::optional<std::size_t>{}:std::optional<std::size_t>{static_cast<std::size_t>(ref[i].owner)}));}
 }
 // One clip is borrowed by all voices: detach every reference before freeing it.
 pool.stop_all();
 for(std::size_t i=0;i<audio_mix::kMaxVoices;++i)CHECK(pool.start(3,*clips[3])==i);
 pool.stop_owner(3);clips[3].reset();
 std::int16_t silence[18];std::fill(std::begin(silence),std::end(silence),42);pool.render(silence,sizeof silence);
 for(auto sample:silence)CHECK(sample==0);
 for(std::size_t i=0;i<audio_mix::kMaxVoices;++i)CHECK(!pool.owner_of(i));
 // Exact end boundary frees the slot immediately; reuse it before stealing.
 CHECK(pool.start(0,*clips[0])==0);pool.render(silence,4);CHECK(!pool.voice_playing(0));CHECK(pool.start(1,*clips[1])==0);
 CHECK(!pool.owner_of(audio_mix::kMaxVoices));
 pool.stop_all(); // End every borrow before the local PCM owners leave scope.
 std::puts("Voice pool: oldest order, 5000 independent-model operations, rejection, shared lifetime and exact-end reuse passed.");
}
