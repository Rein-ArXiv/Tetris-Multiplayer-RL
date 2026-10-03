#include "client/game.h"
#include "simulation/state_hash.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <type_traits>
#define CHECK(e) do{if(!(e)){std::fprintf(stderr,"CHECK failed line %d: %s\n",__LINE__,#e);std::exit(1);}}while(false)
static bool equal_bytes(const study_round::Round&a,const study_round::Round&b){
    const auto x=study_hash::state_bytes(a),y=study_hash::state_bytes(b);
    return x.ok()&&y.ok()&&x.size()==y.size()&&std::equal(x.data(),x.data()+x.size(),y.data());
}
static bool same_piece(const std::optional<study_piece::Piece>&a,const std::optional<study_piece::Piece>&b){
    if(bool(a)!=bool(b))return false;
    if(!a)return true;
    if(a->origin.row!=b->origin.row||a->origin.column!=b->origin.column)return false;
    for(std::size_t i=0;i<4;++i)if(a->local[i].row!=b->local[i].row||a->local[i].column!=b->local[i].column)return false;
    return true;
}
int main(){
    static_assert(std::is_copy_constructible_v<study_game::Game>);
    static_assert(std::is_copy_constructible_v<study_game::GameView>);
    auto initial=study_round::Round::create_seeded(study_grid::Grid{},1);CHECK(initial);
    auto invalid_round=*initial;
    CHECK(const_cast<study_grid::Grid&>(invalid_round.board()).set(0,4,study_grid::Cell::filled));
    const auto invalid_hash=study_hash::state_hash(invalid_round);
    CHECK(!study_game::make_view(invalid_round));
    CHECK(study_hash::state_hash(invalid_round)==invalid_hash);
    study_game::Game game(*initial);auto before=game.view();CHECK(before);
    const auto old=*before;const auto old_hash=study_hash::state_hash(game.round());
    before->board.set(19,0,study_grid::Cell::filled);before->next.pop();before->active->origin.row=8;
    CHECK(study_hash::state_hash(game.round())==old_hash);
    CHECK(game.view()->board.get(19,0)==study_grid::Cell::empty);
    const auto report=game.advance(0.017,{false,false,false,false,true,false});CHECK(report&&report->ticks==1&&report->observations[0].lock);
    CHECK(old.board.cells()==initial->board().cells()&&same_piece(old.active,initial->active()));
    CHECK(!same_piece(old.active,game.view()->active));
    const auto state=game.accent_state();const auto hash=study_hash::state_hash(game.round());
    for(int i=0;i<100;++i){CHECK(game.view());(void)game.background();CHECK(game.accent_state()==state&&study_hash::state_hash(game.round())==hash);}
    auto fork=game;CHECK(equal_bytes(game.round(),fork.round())&&fork.accent_state()==state);
    CHECK(fork.advance(0.017,{true,false,false,false,false,false}));CHECK(study_hash::state_hash(game.round())==hash);
    for(double invalid:{-1.0,std::numeric_limits<double>::quiet_NaN(),std::numeric_limits<double>::infinity()}){
        const auto phase=game.phase();CHECK(!game.advance(invalid,{}));CHECK(game.phase()==phase&&game.accent_state()==state&&study_hash::state_hash(game.round())==hash);
    }
    study_grid::Grid tall;for(int c=1;c<10;++c)CHECK(tall.set(4,c,study_grid::Cell::filled));
    auto fast=study_round::Round::create(tall,study_catalog::Kind::T,1);CHECK(fast);
    study_game::Game batch(*fast);auto many=batch.advance(0.1,{});CHECK(many&&many->ticks==6);
    unsigned locks=0;study_presentation::AccentNoise accent;
    double expected_blue=0.09375;
    for(unsigned i=0;i<many->ticks;++i)if(many->observations[i].lock){++locks;expected_blue=0.09375+0.002*accent.sample();}
    CHECK(locks==2&&batch.round().finished());CHECK(batch.accent_state()==accent.state()&&batch.background().b==expected_blue);
    auto ended=batch.view();CHECK(ended&&!ended->active&&!ended->ghost);
    const auto end_state=batch.accent_state();CHECK(batch.advance(0.1,{}));CHECK(batch.accent_state()==end_state);
    for(std::uint64_t seed=0;seed<100;++seed){
        auto start=study_round::Round::create_seeded(study_grid::Grid{},seed);CHECK(start);
        study_game::Game wrapped(*start,seed+17);study_loop::FrameRunner direct(*start);study_presentation::AccentNoise effect(seed+17);
        double blue=0.09375;
        for(unsigned frame=0;frame<150;++frame){
            const double dt=frame%3==0?0.0:frame%3==1?0.017:0.1;
            const study_loop::FrameInput input{frame%5==0,frame%7==0,frame%11==0,frame%4==0,frame%13==0,frame%29==0};
            const auto a=wrapped.advance(dt,input),b=direct.advance(dt,input);CHECK(a&&b&&a->ticks==b->ticks);
            CHECK(a->board_changed==b->board_changed&&a->piece_changed==b->piece_changed&&a->cleared==b->cleared);
            for(unsigned i=0;i<b->ticks;++i){CHECK(a->observations[i].step==b->observations[i].step);if(b->observations[i].lock)blue=0.09375+0.002*effect.sample();}
            CHECK(equal_bytes(wrapped.round(),direct.round())&&wrapped.phase()==direct.phase());
            CHECK(wrapped.accent_state()==effect.state()&&wrapped.background().b==blue);
            const auto view=wrapped.view();CHECK(view&&view->board.cells()==direct.round().board().cells()&&same_piece(view->active,direct.round().active()));
            CHECK(view->score==direct.round().score()&&view->end_reason==direct.round().end_reason());
            for(std::size_t i=0;i<3;++i)CHECK(view->next.peek(i)==direct.round().next().peek(i));
            const auto ghost=direct.round().ghost();CHECK(bool(view->ghost)==bool(ghost));if(ghost)CHECK(view->ghost->distance==ghost->distance);
        }
    }
    std::puts("Adapter: value snapshots, independent forks, failed frames, two locks per batch, stopped reads and 15k frame delegation/effect parity passed.");
}
