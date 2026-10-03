#include "presentation/idle_animation.h"
#include "client/game.h"
#include "client/menu_model.h"
#include "simulation/state_hash.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
#define CHECK(e) do{if(!(e)){std::fprintf(stderr,"idle line %d: %s\n",__LINE__,#e);std::exit(1);}}while(false)
static bool near(double a,double b){return std::abs(a-b)<1e-10;}
int main(){
 using namespace study_idle;
 Clock clock;CHECK(clock.seconds()==0&&clock.phase()==0);
 CHECK(clock.advance(.0625,true)&&clock.seconds()==.0625);
 for(double bad:{-1.,-std::numeric_limits<double>::infinity(),std::numeric_limits<double>::infinity(),std::numeric_limits<double>::quiet_NaN()})for(bool active:{false,true}){
  CHECK(!clock.advance(bad,active)&&clock.seconds()==.0625);
 }
 CHECK(clock.advance(3.,false)&&clock.seconds()==.0625);
 CHECK(clock.advance(std::numeric_limits<double>::max(),true)&&near(clock.seconds(),.1625));
 Clock a,b;for(int i=0;i<16;++i)CHECK(a.advance(.0625,true));for(int i=0;i<32;++i)CHECK(b.advance(.03125,true));
 CHECK(a.seconds()==1&&a.seconds()==b.seconds()&&a.phase()==.25);
 CHECK(a.advance(3.,true)&&near(a.seconds(),1.1)); // Excess time is discarded, not queued.
 Clock wrap;for(int i=0;i<64000;++i)CHECK(wrap.advance(.0625,true));CHECK(wrap.seconds()==0);
 Animation animation(17),same(17),different(18);
 XorShift64Star expected(17);
 const auto offset1=double(expected.next()>>11)/9007199254740992.0;
 const auto offset2=double(expected.next()>>11)/9007199254740992.0;
 CHECK(animation.border_offset()==offset1&&animation.portrait_offset()==offset2);
 CHECK(offset1>=0&&offset1<1&&offset2>=0&&offset2<1);
 CHECK(different.border_offset()!=offset1||different.portrait_offset()!=offset2);
 for(int frame=0;frame<4096;++frame){
  CHECK(animation.advance(.03125,true)&&same.advance(.03125,true));
  const auto f=animation.sample(true),g=same.sample(true);
  CHECK(f.border_alpha==g.border_alpha&&f.portrait_alpha==g.portrait_alpha&&f.decoration_degrees==g.decoration_degrees);
  CHECK(f.border_alpha>=.65&&f.border_alpha<=1&&f.portrait_alpha>=.85&&f.portrait_alpha<=1);
  CHECK(f.decoration_degrees>=0&&f.decoration_degrees<360);
  const auto seconds=animation.seconds();
  for(int q=0;q<7;++q){const auto r=animation.sample(true);CHECK(r.border_alpha==f.border_alpha&&r.portrait_alpha==f.portrait_alpha&&r.decoration_degrees==f.decoration_degrees);}
  CHECK(animation.seconds()==seconds);
  const auto off=animation.sample(false);CHECK(off.border_alpha==1&&off.portrait_alpha==1&&off.decoration_degrees==30);
 }
 CHECK(animation.advance(.0625,true));const auto before=animation.sample(true);const auto time=animation.seconds();
 for(int i=0;i<50;++i)CHECK(animation.advance(.1,false));
 const auto resumed=animation.sample(true);CHECK(animation.seconds()==time&&before.border_alpha==resumed.border_alpha&&before.decoration_degrees==resumed.decoration_degrees);
 // Independent quarter-cycle expectations for the border sine, after removing its offset.
 const double phases[]={0.,.25,.5,.75},alphas[]={.825,1.,.825,.65};
 for(int i=0;i<4;++i){
  Animation cardinal(17);double remaining=std::fmod(phases[i]-cardinal.border_offset()+1.,1.)*4.;
  while(remaining>.0625){CHECK(cardinal.advance(.0625,true));remaining-=.0625;}
  CHECK(cardinal.advance(remaining,true));CHECK(near(cardinal.sample(true).border_alpha,alphas[i]));
 }
 Animation quarter;for(int i=0;i<16;++i)CHECK(quarter.advance(.0625,true));CHECK(quarter.sample(true).decoration_degrees==90);
 Clock stall,split;CHECK(stall.advance(1.,true));for(int i=0;i<10;++i)CHECK(split.advance(.1,true));
 CHECK(near(stall.seconds(),.1)&&near(split.seconds(),1.));
 // Cosmetic calls are interleaved with genuine rule advancement, not a stationary hash check.
 const auto round=study_round::Round::create_seeded(study_grid::Grid{},123);CHECK(round);
 study_game::Game control(*round),visual(*round);Animation idle(19);std::size_t hashes=0,changed=0,locks=0;auto previous=study_hash::state_hash(control.round());
 for(int i=0;i<600;++i){
  const double dt=i%4==0?1./120:i%4==1?1./60:i%4==2?1./30:.08;
  study_loop::FrameInput input{};input.left=i%31==0;input.right=i%37==0;input.up=i%17==0;input.drop=i%43==0;
  const auto c=control.advance(dt,input);CHECK(c);
  CHECK(idle.advance(dt,i%5!=0));for(int q=0;q<i%9;++q)(void)idle.sample(i%7!=0);
  const auto v=visual.advance(dt,input);CHECK(v&&c->ticks==v->ticks);
  const auto hc=study_hash::state_hash(control.round()),hv=study_hash::state_hash(visual.round());CHECK(hc&&hv&&hc==hv&&control.phase()==visual.phase()&&control.accent_state()==visual.accent_state());if(hc!=previous)++changed;previous=hc;for(unsigned t=0;t<c->ticks;++t)if(c->observations[t].lock)++locks;++hashes;
 }
 CHECK(changed>20&&locks>0);
 // Same seed does not save two consumers that share one mutable engine.
 XorShift64Star clean(42),shared(42);(void)shared.next();CHECK(clean.next()!=shared.next());
 std::printf("Idle: invalid dt/pause/clamp/wrap/partition, 4096 sampled frames with repeated pure queries; %zu advancing rule hashes invariant under cosmetic calls\n",hashes);
}
