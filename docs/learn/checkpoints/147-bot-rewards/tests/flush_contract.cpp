#include "renderer/flush_scene.h"
#include "client/game.h"
#include "simulation/state_hash.h"
#include <vector>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <climits>
#define CHECK(e) do{if(!(e)){std::fprintf(stderr,"CHECK failed line %d: %s\n",__LINE__,#e);std::exit(1);}}while(false)
using namespace study_flush;
struct Packet {Clip clip;std::vector<study_batch::Vertex> vertices;};
struct Sink {
 Clip current{};std::vector<Packet> packets;unsigned clips=0,submits=0;
 bool bad_clip=false,bad_submit=false;
 bool set_clip(Clip c) noexcept {++clips;current=c;return !bad_clip;}
 bool submit(const study_batch::Batch& b) noexcept {
  ++submits;packets.push_back({current,{b.data(),b.data()+b.size()}});return !bad_submit;
 }
};
int main(){
 const Clip a{0,0,320,240},b{110,20,100,200};
 CHECK(valid(a)&&valid({0,0,0,0})&&!valid({-1,0,1,1})&&!valid({INT_MAX,0,1,1}));
 for(study_letterbox::Size size: {study_letterbox::Size{640,480},{1001,479},{319,777},{1,1},{INT_MAX,INT_MAX}}){
  const auto l=study_letterbox::make_layout({320,240},size,{320,240});if(!l){CHECK(size.width==1&&size.height==1);continue;}
  const auto full=full_clip(*l);CHECK(valid(full));
  const auto all=project_clip(*l,{0,0,320,240});CHECK(all&&*all==full);
  CHECK(!project_clip(*l,{-1,0,1,1})&&!project_clip(*l,{320,0,1,1})&&!project_clip(*l,{0,0,0,1})&&!project_clip(*l,{0,0,INT_MAX,1}));
  const auto c=project_clip(*l,{110,20,100,200});CHECK(c&&valid(*c));
  CHECK(c->x>=full.x&&c->y>=full.y&&c->x+c->width<=full.x+full.width&&c->y+c->height<=full.y+full.height);
  // Independent rational edge bounds: outward rounding differs by < 1 pixel.
  const double left=double(l->viewport.x)+110.*l->viewport.width/320.;
  const double right=double(l->viewport.x)+210.*l->viewport.width/320.;
  const double low=double(l->drawable.height-l->viewport.y)-220.*l->viewport.height/240.;
  const double high=double(l->drawable.height-l->viewport.y)-20.*l->viewport.height/240.;
  CHECK(c->x<=left&&left-c->x<1&&c->x+c->width>=right&&c->x+c->width-right<1);
  CHECK(c->y<=low&&low-c->y<1&&c->y+c->height>=high&&c->y+c->height-high<1);
  if(size.width==640)CHECK((*c==Clip{220,40,200,400}));
 }
 study_mesh::Vertex2 tri[]={{-.5f,-.5f},{.5f,-.5f},{0,.5f}};
 const study_batch::Color red{1,0,0,1},green{0,1,0,1};
 study_batch::Batch merge;CHECK(merge.append(tri,3,red)&&merge.append_batch(merge));
 CHECK(merge.size()==6&&std::memcmp(merge.data(),merge.data()+3,3*sizeof(study_batch::Vertex))==0);
 study_batch::Batch huge;for(unsigned i=0;i<768;++i)CHECK(huge.append(tri,3,green));
 CHECK(!merge.append_batch(huge)&&merge.size()==6&&merge.data()[5].r==1);
 Sink sink;Stream<Sink> s(sink,a);CHECK(s.flush()&&s.set_clip(b)&&sink.clips==0);
 CHECK(s.append(tri,3,red)&&s.set_clip(b)&&sink.submits==0);
 CHECK(s.append(tri,3,green)&&s.set_clip(a));CHECK(sink.packets.size()==1&&sink.packets[0].clip==b&&sink.packets[0].vertices.size()==6);
 CHECK(sink.packets[0].vertices[0].r==1&&sink.packets[0].vertices[3].g==1);
 CHECK(s.append(tri,3,red)&&s.set_clip(b)&&s.append(tri,3,green)&&s.finish());
 CHECK(sink.packets.size()==3&&sink.packets[1].clip==a&&sink.packets[2].clip==b);
 CHECK(s.statistics().draws==3&&s.statistics().vertices==12&&s.pending()==0);
 CHECK(s.finish()&&!s.append(tri,3,red)&&!s.set_clip(a)&&sink.submits==3);
 Sink cap;Stream<Sink> c(cap,a);for(unsigned i=0;i<768;++i)CHECK(c.append(tri,3,red));
 CHECK(cap.submits==0&&c.pending()==2304);CHECK(c.append(tri,3,green));
 CHECK(cap.submits==1&&cap.packets[0].vertices.size()==2304&&c.pending()==3);
 CHECK(c.finish()&&cap.packets[1].vertices[0].g==1&&c.statistics().vertices==2307);
 // A bad new group must not trigger a capacity flush or partially append.
 for(unsigned failure=0;failure<3;++failure){Sink t;Stream<Sink> stream(t,a);CHECK(stream.append(tri,3,red));
  if(failure==0){auto bad=tri[2];tri[2].x=NAN;CHECK(!stream.append(tri,3,green));tri[2]=bad;}
  if(failure==1)CHECK(!stream.append(tri,2307,green));
  if(failure==2)CHECK(!stream.set_clip({-1,0,0,0}));
  CHECK(!stream.good()&&stream.pending()==3&&!stream.finish()&&t.submits==0);
 }
 Sink destructor;{Stream<Sink> pending(destructor,a);CHECK(pending.append(tri,3,red));}CHECK(destructor.submits==0);
 Sink empty;Stream<Sink> e(empty,a);CHECK(e.append(nullptr,0,{NAN,NAN,NAN,NAN})&&e.finish()&&empty.clips==0);
 for(bool fail_clip:{true,false}){Sink t;t.bad_clip=fail_clip;t.bad_submit=!fail_clip;Stream<Sink> f(t,a);
  CHECK(f.append(tri,3,red)&&!f.set_clip(b)&&!f.good());const auto submits=t.submits,clips=t.clips;
  CHECK(!f.flush()&&!f.finish()&&!f.append(tri,3,red)&&t.submits==submits&&t.clips==clips);
  CHECK(submits==(fail_clip?0u:1u)&&f.statistics().draws==0&&f.pending()==3);
 }
 Sink invalid;Stream<Sink> bad(invalid,{-1,0,1,1});CHECK(!bad.good()&&!bad.finish()&&invalid.clips==0);
 for(std::uint64_t seed=0;seed<20;++seed){auto r=study_round::Round::create_seeded(study_grid::Grid{},seed);CHECK(r);study_game::Game game(*r);
  for(unsigned frame=0;frame<100;++frame){CHECK(game.advance(.017,{frame%5==0,frame%7==0,frame%11==0,frame%3==0,frame%13==0,false}));
   const auto h=study_hash::state_hash(game.round());auto v=game.view();CHECK(v);study_batch::Batch reference;CHECK(study_batch::scene(reference,*v));
   Sink t;Stream<Sink> stream(t,a);CHECK(study_flush::scene(stream,*v,a,b)&&stream.finish());
   CHECK(t.packets.size()==2&&t.packets[0].clip==b&&t.packets[1].clip==a);std::vector<study_batch::Vertex> joined;
   for(const auto& packet:t.packets)joined.insert(joined.end(),packet.vertices.begin(),packet.vertices.end());
   CHECK(joined.size()==reference.size()&&std::memcmp(joined.data(),reference.data(),joined.size()*sizeof(study_batch::Vertex))==0);
   CHECK(study_hash::state_hash(game.round())==h);
  }
 }
 std::puts("Clip projection, ordered boundaries, capacity, failure/no-retry, explicit finish and 2000 scene compositions passed");
}
