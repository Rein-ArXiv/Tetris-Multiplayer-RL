#include "presentation/sound_policy.h"
#include "client/game.h"
#include "simulation/state_hash.h"
#include <stdexcept>
#include <cstdio>
#include <cstdlib>
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"failure policy %d: %s\n",__LINE__,#x);std::exit(1);}}while(false)
using namespace study_sound;
struct Device {
 inline static int opens=0,installs=0,plays=0,stops=0,closes=0,live=0,fail_install=-1;
 inline static bool fail_open=false,fail_play=false;
 bool active=false;
 static void reset(){CHECK(live==0);opens=installs=plays=stops=closes=0;fail_install=-1;fail_open=fail_play=false;}
 bool open(unsigned rate,unsigned channels) noexcept {CHECK(rate==44100&&channels==2);++opens;if(fail_open)return false;active=true;++live;return true;}
 bool replace(study_audio::Pcm16&& p,std::size_t slot) noexcept {CHECK(active&&p.rate()==44100);++installs;return int(slot)!=fail_install;}
 bool play(std::size_t slot) noexcept{CHECK(active&&slot<4);++plays;return !fail_play;}
 void stop() noexcept{CHECK(active);++stops;}
 void close() noexcept{++closes;if(active){--live;active=false;}}
};
using Session=BasicSession<Device,true>;
static int factory_calls=0,factory_fail=-1,factory_mode=0;
static std::optional<study_audio::Pcm16> factory(Kind kind){
 ++factory_calls;if(int(kind)==factory_fail){if(factory_mode==1)throw std::bad_alloc{};if(factory_mode==2)throw std::runtime_error("injected");return {};}
 return study_audio::make_cue(kind);
}
static void expect(Session&s,Failure why){CHECK(s.state()==AudioState::unavailable);CHECK(s.take_notice()==why);CHECK(!s.take_notice());CHECK(!s.prepare());CHECK(s.send(Kind::rotate)==Delivery::skipped);CHECK(Device::plays==0&&Device::live==0);}
int main(){
 once_flags::Flags<3> flags;CHECK(!flags.take(3));for(int i=0;i<3;++i){CHECK(flags.take(i));CHECK(!flags.take(i));}flags.reset();CHECK(flags.take(0));
 {struct Absent{};BasicSession<Absent,false> off;CHECK(!off.prepare()&&!off.prepare());CHECK(off.state()==AudioState::disabled);CHECK(off.send(Kind::drop)==Delivery::skipped);CHECK(!off.take_notice());off.stop();}
 Device::reset();{Session s;CHECK(s.state()==AudioState::unprepared);CHECK(s.send(Kind::drop)==Delivery::skipped);CHECK(!s.take_notice());Device::fail_open=true;CHECK(!s.prepare());expect(s,Failure::open);CHECK(Device::opens==1);}
 for(int mode=0;mode<3;++mode)for(int slot=0;slot<4;++slot){
  Device::reset();factory_calls=0;factory_fail=slot;factory_mode=mode;
  {Session s;CHECK(!s.prepare(factory));expect(s,mode==0?Failure::clip:mode==1?Failure::memory:Failure::unexpected);CHECK(factory_calls==slot+1&&Device::installs==slot);}
 }
 factory_fail=-1;
 for(int slot=0;slot<4;++slot){Device::reset();Device::fail_install=slot;{Session s;CHECK(!s.prepare());expect(s,Failure::install);CHECK(Device::installs==slot+1);}}
 Device::reset();{Session s;CHECK(!s.prepare(nullptr));expect(s,Failure::clip);CHECK(Device::opens==0);}
 Device::reset();{
  Session s;CHECK(s.prepare()&&s.prepare());CHECK(Device::opens==1&&Device::installs==4);
  CHECK(s.send(static_cast<Kind>(-1))==Delivery::invalid_kind);CHECK(s.take_notice()==Failure::invalid_kind);CHECK(Device::plays==0);
  CHECK(s.send(static_cast<Kind>(4))==Delivery::invalid_kind);CHECK(!s.take_notice());
  Device::fail_play=true;for(int i=0;i<1000;++i)CHECK(s.send(Kind::rotate)==Delivery::failed);
  CHECK(s.state()==AudioState::ready&&s.take_notice()==Failure::play&&!s.take_notice());
  CHECK(s.send(Kind::rotate)==Delivery::failed&&!s.take_notice());Device::fail_play=false;CHECK(s.send(Kind::drop)==Delivery::started);
  s.stop();s.stop();CHECK(Device::stops==2&&s.state()==AudioState::ready);
 }
 CHECK(Device::live==0);
 // Same actual rule runner, sampled input and dt; exercise real Session policy.
 for(int mode=0;mode<3;++mode)for(unsigned seed=0;seed<24;++seed){
  Device::reset();Device::fail_open=mode==1;Device::fail_play=mode==2;
  Session audio;(void)audio.prepare();auto initial=study_round::Round::create_seeded(study_grid::Grid{},seed);CHECK(initial);
  study_game::Game game(*initial),oracle(*initial);std::size_t attempts=0;
  for(unsigned frame=0;frame<180;++frame){
   const study_loop::FrameInput input{frame%5==0,frame%7==0,frame%11==0,frame%4==0,frame%13==0,false};
   const double dt=frame%3==0?0:frame%3==1?.017:.1;
   auto a=game.advance(dt,input),b=oracle.advance(dt,input);CHECK(a&&b);auto batch=Batch::from(*a);CHECK(batch);
   const auto expected=batch->remaining();const auto report=drain(*batch,audio);attempts+=report.consumed;
   CHECK(report.consumed==expected&&expected<=24&&batch->empty());
   CHECK(report.consumed==report.started+report.skipped+report.failed+report.invalid);
   CHECK((mode==0?report.started:mode==1?report.skipped:report.failed)==expected);
   CHECK(drain(*batch,audio).consumed==0);
   CHECK(study_hash::state_hash(game.round())==study_hash::state_hash(oracle.round()));CHECK(game.phase()==oracle.phase());
  }
  CHECK(attempts>0);CHECK(Device::plays==int(mode==1?0:attempts));
 }
 CHECK(Device::live==0);
 study_loop::FrameReport corrupt;corrupt.ticks=study_loop::max_ticks+1;CHECK(!Batch::from(corrupt));
 std::puts("Failure policy: every preparation boundary, bounded notices, disabled build, 12960 matched frames, consumed failed cues and invariant rejection passed.");
}
