#include "presentation/sound_events.h"
#include "client/game.h"
#include "simulation/state_hash.h"
#include <cstdio>
#include <cstdlib>
#include <type_traits>
#include <vector>
#define CHECK(e) do { if(!(e)){std::fprintf(stderr,"line %d: %s\n",__LINE__,#e);std::exit(1);} } while(false)
using study_sound::Kind;
using study_sound::Batch;
static std::vector<Kind> drain(Batch& b){
    std::vector<Kind> out;
    while(auto k=b.take()) out.push_back(*k);
    return out;
}
static void transfer(Batch& to,Batch& from){to=std::move(from);}
int main(){
    static_assert(!std::is_copy_constructible_v<Batch>);
    static_assert(std::is_nothrow_move_constructible_v<Batch>);
    study_loop::FrameReport f;
    auto empty=Batch::from(f);CHECK(empty&&empty->empty());
    f.ticks=study_loop::max_ticks;
    for(auto& o:f.observations){
        o.step=study_round::Step::locked;o.kick=0;
        o.lock.emplace();o.lock->hard_drop_distance=0;
        o.lock->cleared=4;o.lock->inserted=8;
    }
    auto full=Batch::from(f);CHECK(full&&full->remaining()==24);
    CHECK(full->take()==Kind::rotate);
    Batch moved(std::move(*full));CHECK(full->empty()&&!full->take());
    Batch assigned;assigned=std::move(moved);CHECK(moved.empty()&&assigned.remaining()==23);
    transfer(assigned,assigned);CHECK(assigned.remaining()==23);
    std::vector<Kind> expected{Kind::drop,Kind::clear,Kind::garbage};
    for(int i=1;i<6;++i)for(Kind k:{Kind::rotate,Kind::drop,Kind::clear,Kind::garbage})expected.push_back(k);
    CHECK(drain(assigned)==expected&&drain(assigned).empty());
    f.ticks=7;CHECK(!Batch::from(f));
    f.ticks=2;f.observations[0].step=study_round::Step::stopped;
    f.observations[1].step=study_round::Step::invalid;
    CHECK(Batch::from(f)->empty());
    f.ticks=1;f.observations[0]={};f.observations[0].lock.emplace();
    CHECK(Batch::from(f)->empty()); // -1 sentinels; zero clear/inserted

    // Real game report: zero-tick pending input then accepted rotation/drop.
    auto initial=study_round::Round::create_seeded(study_grid::Grid{},1);CHECK(initial);
    study_game::Game game(*initial);
    auto zero=game.advance(0.0,{false,false,true,false,true,false});CHECK(zero&&zero->ticks==0);
    CHECK(Batch::from(*zero)->empty());
    auto accepted=game.advance(.017,{});CHECK(accepted&&accepted->ticks==1);
    auto cues=Batch::from(*accepted);CHECK(cues);
    CHECK((drain(*cues)==std::vector<Kind>{Kind::rotate,Kind::drop}));
    const auto hash=study_hash::state_hash(game.round());
    for(int i=0;i<100;++i){CHECK(game.view());CHECK(cues->empty());}
    CHECK(study_hash::state_hash(game.round())==hash);
    CHECK(!game.advance(-1.0,{}));
    auto next=game.advance(.017,{});CHECK(next&&Batch::from(*next)->empty());

    // Six tick reports retain both real lock observations.
    study_grid::Grid tall;for(int c=1;c<10;++c)CHECK(tall.set(4,c,study_grid::Cell::filled));
    auto fast=study_round::Round::create(tall,study_catalog::Kind::T,1);CHECK(fast);
    study_game::Game batch(*fast);auto many=batch.advance(.1,{});CHECK(many&&many->ticks==6);
    unsigned locks=0;for(unsigned i=0;i<many->ticks;++i)if(many->observations[i].lock)++locks;
    CHECK(locks==2); // default policy gives plain lock no cue
    CHECK(Batch::from(*many)->empty());

    // Sound-consumer policy (including failed attempts) cannot alter rule state.
    for(unsigned seed=0;seed<30;++seed){
        auto start=study_round::Round::create_seeded(study_grid::Grid{},seed);CHECK(start);
        study_game::Game audible(*start),silent(*start);
        for(unsigned frame=0;frame<120;++frame){
            study_loop::FrameInput input{frame%5==0,frame%7==0,frame%11==0,frame%4==0,frame%13==0,false};
            double dt=frame%3==0?0.0:frame%3==1?.017:.1;
            auto a=audible.advance(dt,input),b=silent.advance(dt,input);CHECK(a&&b);
            auto sound=Batch::from(*a);CHECK(sound);
            while(sound->take()) {} // Attempt fails: still consumed, no retry.
            CHECK(study_hash::state_hash(audible.round())==study_hash::state_hash(silent.round()));
            CHECK(audible.phase()==silent.phase());
        }
    }
    std::puts("Sound reports: 24 bound, zero sentinels, ordering, move/drain, pending input, views, 3600 frame parity passed.");
}
