#include "audio/voice.h"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <vector>
#define CHECK(e) do{if(!(e)){std::fprintf(stderr,"voice line %d: %s\n",__LINE__,#e);std::exit(1);}}while(false)
int main(){
 using namespace study_audio;
 const auto clip=Pcm16::make(48000,2,{1,-1,2,-2,3,-3});CHECK(clip);
 Voice voice;std::array<unsigned char,19> out;out.fill(0xCC);
 voice.render(out.data()+1,9);for(int i=1;i<=9;++i)CHECK(out[i]==0);CHECK(out[0]==0xCC&&out[10]==0xCC);
 voice.start(*clip);voice.render(nullptr,0);CHECK(voice.playing()&&voice.cursor_frames()==0);
 out.fill(0xCC);voice.render(out.data()+1,9);CHECK(voice.cursor_frames()==2&&voice.playing());
 CHECK(std::memcmp(out.data()+1,clip->samples().data(),8)==0&&out[9]==0&&out[0]==0xCC&&out[10]==0xCC);
 out.fill(0xCC);voice.render(out.data()+1,16);CHECK(voice.cursor_frames()==3&&!voice.playing());
 CHECK(std::memcmp(out.data()+1,clip->samples().data()+4,4)==0);for(int i=5;i<=16;++i)CHECK(out[i]==0);CHECK(out[0]==0xCC&&out[17]==0xCC);
 voice.stop();CHECK(!voice.playing()&&voice.cursor_frames()==0);out.fill(0xCC);voice.render(out.data(),out.size());for(auto x:out)CHECK(x==0);
 Voice a,b;a.start(*clip);b.start(*clip);a.render(out.data(),4);CHECK(a.cursor_frames()==1&&b.cursor_frames()==0);b.render(out.data(),8);CHECK(a.cursor_frames()==1&&b.cursor_frames()==2);a.stop();b.stop();
 // Byte-sized requests can be unaligned and can include an incomplete tail.
 for(unsigned channels:{1u,2u})for(std::size_t frames=1;frames<=31;++frames)for(std::size_t request=1;request<40;++request){
  std::vector<int16_t> samples(frames*channels);for(std::size_t i=0;i<samples.size();++i)samples[i]=static_cast<int16_t>(static_cast<int>(i)*3-50);
  auto pcm=Pcm16::make(44100,channels,std::move(samples));CHECK(pcm);Voice v;v.start(*pcm);std::vector<unsigned char> bytes(request+2,0xAB);v.render(bytes.data()+1,request);
  const auto n=std::min(frames,request/(channels*2));CHECK(v.cursor_frames()==n);CHECK(bytes.front()==0xAB&&bytes.back()==0xAB);CHECK(std::memcmp(bytes.data()+1,pcm->samples().data(),n*channels*2)==0);
  for(std::size_t i=n*channels*2;i<request;++i)CHECK(bytes[i+1]==0);
 }
 std::puts("Voice: independent cursors, frame units, unaligned bytes, zero tails and stop/detach passed.");
}
