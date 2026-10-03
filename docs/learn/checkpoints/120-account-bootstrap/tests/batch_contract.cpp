#include "renderer/batch_scene.h"
#include "client/application.h"
#include "simulation/state_hash.h"
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <vector>
#define CHECK(e) do{if(!(e)){std::fprintf(stderr,"CHECK failed line %d: %s\n",__LINE__,#e);std::exit(1);}}while(false)
using namespace study_batch;
int main(){
    study_mesh::Vertex2 tri[]={{-.5f,-.5f},{.5f,-.5f},{0,.5f}};
    Batch b;CHECK(b.append(tri,3,{1,0,0,1}));CHECK(b.size()==3);
    const auto saved=b;
    for(std::size_t count:{1u,2u,4u,2307u}){CHECK(!b.append(tri,count,{1,0,0,1}));CHECK(b.size()==saved.size());}
    CHECK(!b.append(nullptr,3,{1,0,0,1}));CHECK(b.append(nullptr,0,{NAN,NAN,NAN,NAN}));
    auto bad=tri[2];tri[2].y=std::numeric_limits<float>::infinity();
    CHECK(!b.append(tri,3,{0,1,0,1}));tri[2]=bad;
    for(Color c: {Color{-1,0,0,1},Color{0,NAN,0,1},Color{0,0,2,1},Color{0,0,0,.5f},Color{0,0,0,NAN}}) CHECK(!b.append(tri,3,c));
    CHECK(b.size()==saved.size()&&std::memcmp(b.data(),saved.data(),3*sizeof(Vertex))==0);
    tri[0].x=.25f;CHECK(b.data()[0].x==-.5f); // Copies, does not retain caller storage.
    b.clear();for(std::size_t i=0;i<Batch::capacity/3;++i)CHECK(b.append(tri,3,{0,1,0,1}));
    CHECK(b.size()==Batch::capacity);CHECK(!b.append(tri,3,{1,0,0,1}));CHECK(b.data()[2303].g==1);
    CHECK(!b.append(tri,std::numeric_limits<std::size_t>::max(),{1,0,0,1}));
    b.clear();CHECK(b.append(tri,3,{1,0,0,1})&&b.size()==3&&b.data()[0].r==1);
    CHECK(menu(b)&&b.size()==3);
    auto initial=study_round::Round::create_seeded(study_grid::Grid{},1);CHECK(initial);
    study_game::Game game(*initial);const auto hash=study_hash::state_hash(game.round());
    auto view=game.view();CHECK(view&&scene(b,*view));CHECK(study_hash::state_hash(game.round())==hash);
    CHECK(b.size()==1356); // 1200 board + 24 ghost + 24 active + 72 next + 36 score zero.
    CHECK(b.data()[0].r==empty_color.r&&b.data()[1200].r==ghost_color.r);
    CHECK(b.data()[1224].g==piece_color.g&&b.data()[1320].g==score_color.g);
    const auto prior=b;view->ghost.reset();CHECK(!scene(b,*view));CHECK(b.size()==prior.size()&&std::memcmp(b.data(),prior.data(),b.size()*sizeof(Vertex))==0);
    for(std::uint64_t seed=0;seed<50;++seed){
        auto round=study_round::Round::create_seeded(study_grid::Grid{},seed);CHECK(round);study_game::Game g(*round);
        for(unsigned frame=0;frame<100;++frame){
            CHECK(g.advance(.017,{frame%5==0,frame%7==0,frame%11==0,frame%3==0,frame%13==0,false}));
            const auto h=study_hash::state_hash(g.round());const auto v=g.view();CHECK(v&&scene(b,*v));
            CHECK(b.size()<=Batch::capacity&&b.size()%3==0&&study_hash::state_hash(g.round())==h);
            for(std::size_t i=0;i<b.size();++i)CHECK(b.data()[i].a==1&&std::isfinite(b.data()[i].x)&&std::isfinite(b.data()[i].y));
        }
    }
    auto huge=game.view();CHECK(huge);huge->score=std::numeric_limits<std::uint64_t>::max();CHECK(scene(b,*huge)&&b.size()<=2172);
    std::puts("Batch boundaries, atomic append, copied data, ordered scene and 5000 frame compositions passed");
}
