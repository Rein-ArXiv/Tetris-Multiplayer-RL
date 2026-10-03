#include "content/art_set.h"
#include "client/menu_model.h"
#include "presentation/image_fit.h"
#include "simulation/round.h"
#include "simulation/state_hash.h"
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <limits>
#define CHECK(e) do{if(!(e)){std::fprintf(stderr,"character line %d: %s\n",__LINE__,#e);std::exit(1);}}while(false)
int main(){
 CHECK(study_characters::catalog_valid()&&study_characters::find("player")&&study_characters::find("rook"));
 CHECK(!study_characters::find("")&&!study_characters::find("unknown"));
 study_menu::Preferences prefs;CHECK(prefs.character_id()=="player");
 {std::string temporary="rook";CHECK(prefs.select_character(temporary));temporary.assign(100,'x');}
 CHECK(prefs.character_id()=="rook"&&prefs.character_id().data()==study_characters::find("rook")->id.data());
 CHECK(prefs.select_character("rook")&&!prefs.select_character("missing")&&prefs.character_id()=="rook");
 const auto round=study_round::Round::create_seeded(study_grid::Grid{},42);CHECK(round);const auto hash=study_hash::state_hash(*round);
 for(int failures=0;failures<8;++failures){
  std::array<int,3> calls{};study_art::ArtSet set;CHECK(!set.resolve("rook"));
  const auto load=[&](const std::string& path)->study_art::Handle {
   const int index=path=="assets/player.png"?0:path=="assets/bot.png"?1:path=="assets/opponent.png"?2:-1;
   CHECK(index>=0);++calls[index];return failures&(1<<index)?0:study_art::Handle(10+index);
  };
  CHECK(!set.init(load,0));CHECK(calls[0]+calls[1]+calls[2]==0);
  CHECK(set.init(load,99));CHECK(calls[0]==1&&calls[1]==1&&calls[2]==1);
  const auto* player=set.resolve("player");const auto* rook=set.resolve("rook");CHECK(player&&rook);
  CHECK(player->icon==((failures&1)?99:10)&&player->portrait==player->icon);
  CHECK(player->icon_fallback==bool(failures&1)&&player->portrait_fallback==player->icon_fallback);
  CHECK(rook->icon==((failures&2)?99:11)&&rook->portrait==((failures&4)?99:12));
  CHECK(rook->icon_fallback==bool(failures&2)&&rook->portrait_fallback==bool(failures&4));
  for(int i=0;i<100;++i){CHECK(set.resolve("rook")==rook);CHECK(!set.resolve("missing"));CHECK(prefs.character_id()=="rook");}
  CHECK(!set.init(load,99)&&calls[0]==1&&calls[1]==1&&calls[2]==1);
  CHECK(study_hash::state_hash(*round)==hash);
 }
 study_art::ArtSet failed;int calls=0;try{failed.init([&](const std::string&)->study_art::Handle{if(++calls==2)throw std::runtime_error("decode failure");return 10;},99);CHECK(false);}catch(const std::runtime_error&){}
 CHECK(!failed.ready()&&!failed.resolve("player")&&calls==2);
 std::size_t combinations=0,omitted=0;
 for(int bw=1;bw<=24;++bw)for(int bh=1;bh<=24;++bh)for(int iw=1;iw<=32;++iw)for(int ih=1;ih<=32;++ih){
  const auto r=image_fit::contain({-7,11,bw,bh},iw,ih);
  if(!r){CHECK(bw*ih<iw||bh*iw<ih);++omitted;}
  else{
   CHECK(r->width>0&&r->height>0&&r->width<=bw&&r->height<=bh);
   CHECK(r->width==bw||r->height==bh);
   const int left=r->x+7,top=r->y-11,right=bw-left-r->width,bottom=bh-top-r->height;
   CHECK(left>=0&&top>=0&&right-left>=0&&right-left<=1&&bottom-top>=0&&bottom-top<=1);
   if(r->width==bw){CHECK(r->height*iw<=bw*ih&&(r->height+1)*iw>bw*ih);}
   else {CHECK(r->width*ih<=bh*iw&&(r->width+1)*ih>bh*iw);}
  }++combinations;
 }
 const int hi=(std::numeric_limits<int>::max)(),lo=(std::numeric_limits<int>::min)();
 CHECK(!image_fit::contain({hi,0,1,1},1,1));CHECK(!image_fit::contain({0,hi,1,1},1,1));
 CHECK(!image_fit::contain({0,0,-1,2},1,1));CHECK(!image_fit::contain({0,0,2,2},0,1));
 const auto large=image_fit::contain({lo,lo,hi,hi},hi,hi);CHECK(large&&large->width==hi&&large->x==lo);
 const auto portrait=image_fit::contain({0,0,100,80},20,40);CHECK(portrait&&portrait->x==30&&portrait->y==0&&portrait->width==40&&portrait->height==80);
 std::printf("Character art: 8 failure combinations, path dedup/identity/borrowed views/failed publication; %zu integer fit combinations (%zu omitted), bounds/centering/quantization and unchanged rules passed\n",combinations,omitted);
}
