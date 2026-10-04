#include "audio/xaudio_player.h"
#include "tests/xaudio_fake/fake_xaudio.h"
#include <type_traits>
#define CHECK(e) do{if(!(e)){std::fprintf(stderr,"XAudio contract %d: %s\n",__LINE__,#e);std::exit(1);}}while(false)
namespace f=fake_xaudio;
using study_audio::XAudioPlayer;using study_audio::Pcm16;
static Pcm16 clip(int value=7,std::uint32_t rate=44100){auto p=Pcm16::make(rate,2,std::vector<std::int16_t>(32,static_cast<std::int16_t>(value)));CHECK(p);return std::move(*p);}
int main(){
 static_assert(!std::is_move_constructible_v<XAudioPlayer>);
 for(auto fail:{f::Fail::com,f::Fail::engine,f::Fail::master}){
  f::reset();f::fail=fail;XAudioPlayer p;CHECK(!p.open(44100,2));p.close();p.close();
  CHECK(f::engines==0&&f::masters==0&&f::sources==0&&f::com_refs==0);
  CHECK(f::uninits==(fail==f::Fail::com?0:1));
 }
 for(HRESULT initialized:{S_OK,S_FALSE,RPC_E_CHANGED_MODE}){
  f::reset();f::com_result=initialized;const int host=initialized==S_OK?0:2;f::com_refs=host;
  {XAudioPlayer p;CHECK(!p.open(0,2)&&!p.open(44100,3));CHECK(f::inits==0);
   CHECK(p.open(44100,2));CHECK(!p.open(44100,2));CHECK(f::inits==1);
   CHECK(p.replace(clip()));CHECK(p.play());f::read_all();}
  CHECK(f::engines==0&&f::masters==0&&f::sources==0&&f::com_refs==host);
  CHECK(f::uninits==(initialized==RPC_E_CHANGED_MODE?0:1));
 }
 f::reset();
 {
  XAudioPlayer p;CHECK(!p.play()&&!p.replace(clip()));CHECK(p.open(44100,2));
  for(std::size_t i=0;i<XAudioPlayer::kVoiceCount;++i)CHECK(p.replace(clip(static_cast<int>(i+1)),i));
  CHECK(!p.replace(clip(),XAudioPlayer::kVoiceCount));CHECK(!p.replace(clip(99,48000),0));
  CHECK(!p.play(XAudioPlayer::kVoiceCount));
  for(std::size_t i=0;i<XAudioPlayer::kVoiceCount;++i)CHECK(p.play(i));
  CHECK(f::sources==9&&f::readers_of(1)==1);
  auto wrong=clip(99,48000);
  CHECK(!p.replace(std::move(wrong),0));
  CHECK(!wrong.samples().empty()&&f::sources==9&&f::readers_of(1)==1);
  f::read_all(); // Rejected format preserved the active old PCM and the input.
  CHECK(p.play(8));CHECK(f::readers_of(1)==0&&f::readers_of(9)==2);
  CHECK(p.play(8));CHECK(f::readers_of(2)==0&&f::readers_of(9)==3);
  p.unload(8);CHECK(f::sources==6&&f::readers_of(9)==0);f::read_all();CHECK(!p.play(8));
  CHECK(p.replace(clip(20),2));CHECK(f::readers_of(3)==0);f::read_all();
  p.stop();CHECK(f::sources==0);CHECK(p.play(2));CHECK(f::readers_of(20)==1);
  const auto allocations=f::source_objects.size();f::finish_all();CHECK(p.play(2));
  CHECK(f::source_objects.size()==allocations); // idle compatible source reused
  p.stop();
  for(auto fail:{f::Fail::source,f::Fail::submit,f::Fail::start}){
   f::fail=fail;CHECK(!p.play(2));CHECK(f::sources==0);f::read_all();
   f::fail=f::Fail::none;CHECK(p.play(2));p.stop();
  }
  // A failed full-pool replacement ends exactly the stolen borrow.
  for(std::size_t i=0;i<9;++i)CHECK(p.play(2));
  f::fail=f::Fail::source;CHECK(!p.play(2));CHECK(f::sources==8);
  CHECK(f::readers_of(20)==8);f::read_all();
  f::fail=f::Fail::none;CHECK(p.play(2));CHECK(f::sources==9);p.stop();
  // Two independent graphs share this thread's COM initialization balance.
  XAudioPlayer other;CHECK(other.open(48000,2));CHECK(other.replace(clip(80,48000)));CHECK(other.play());
  p.close();CHECK(f::engines==1&&f::masters==1&&f::sources==1&&f::com_refs==1);f::read_all();
  other.close();CHECK(f::com_refs==0);
  CHECK(p.open(44100,1));p.close();
 }
 CHECK(f::engines==0&&f::masters==0&&f::sources==0&&f::com_refs==0);
 std::puts("XAudio full implementation: COM balances, partial cleanup, format units, pooled ownership, reuse and every failure stage passed (API double).");
}
