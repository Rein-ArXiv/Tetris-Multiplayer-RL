#include "client/application.h"
#include "simulation/state_hash.h"
#include <cstdio>
#include <cstdlib>
#include <limits>
#define CHECK(e) do{if(!(e)){std::fprintf(stderr,"CHECK failed line %d: %s\n",__LINE__,#e);std::exit(1);}}while(false)
using namespace study_app;
static auto digest(const Application& a){CHECK(a.game());return study_hash::state_hash(a.game()->round());}
int main(){
    // Independent complete table: rows are screens, columns are commands.
    const Screen expected[4][3]={{Screen::menu,Screen::playing,Screen::quitting},
      {Screen::playing,Screen::playing,Screen::menu},
      {Screen::finished,Screen::playing,Screen::menu},
      {Screen::quitting,Screen::quitting,Screen::quitting}};
    for(int s=0;s<4;++s)for(int c=0;c<3;++c){
        const auto t=transition(static_cast<Screen>(s),static_cast<Command>(c));
        CHECK(t.next==expected[s][c]);CHECK(t.recreate==(c==1&&(s==0||s==2)));
        CHECK(t.release==(c==2&&s!=3));
    }
    const auto initial=study_round::Round::create_seeded(study_grid::Grid{},1);CHECK(initial);
    const auto baseline=study_hash::state_hash(*initial);CHECK(baseline);
    Application app(*initial);CHECK(app.screen()==Screen::menu&&!app.game());
    CHECK(app.advance(100,{},false,false,true)&&!app.game());
    auto start=app.advance(.1,{false,false,false,true,true,false},true,false,false);
    CHECK(start&&start->game_replaced&&!start->frame&&app.screen()==Screen::playing);
    CHECK(digest(app)==baseline&&app.game()->phase()==0&&!app.controls_armed());
    auto held=app.advance(0,{false,false,false,true,true,false},false,false,false);
    CHECK(held&&held->frame&&held->frame->ticks==0&&!app.controls_armed());
    CHECK(app.advance(0,{},false,false,true)&&app.controls_armed());
    auto tick=app.advance(.017,{},false,false,true);CHECK(tick&&tick->frame->ticks==1);
    CHECK(!tick->frame->observations[0].lock); // Start/held drop did not leak.
    CHECK(app.game()->round().active()->origin.row==initial->active()->origin.row);
    CHECK(app.advance(0,{true,false,false,false,true,false},false,false,false));
    auto back=app.advance(.1,{},true,true,false); // Back wins; no tick before destruction.
    CHECK(back&&!back->frame&&!app.game()&&app.screen()==Screen::menu);
    CHECK(app.advance(.1,{},true,false,false));CHECK(digest(app)==baseline&&app.game()->phase()==0);
    CHECK(app.advance(0,{},false,false,true));
    tick=app.advance(.017,{},false,false,true);CHECK(tick&&!tick->frame->observations[0].lock);
    CHECK(app.game()->round().active()->origin.column==initial->active()->origin.column);
    // Focus loss cancels pending zero-tick requests, even with new raw controls.
    CHECK(app.advance(0,{true,false,false,false,true,false},false,false,false));
    CHECK(app.advance(0,{true,false,false,false,true,true},false,false,false));
    CHECK(!app.controls_armed());CHECK(app.advance(0,{},false,false,true));
    tick=app.advance(.017,{},false,false,true);CHECK(tick&&!tick->frame->observations[0].lock);
    for(double dt:{-1.,std::numeric_limits<double>::quiet_NaN(),std::numeric_limits<double>::infinity()}){
        const auto hash=digest(app);const auto phase=app.game()->phase();
        CHECK(!app.advance(dt,{},false,true,true));CHECK(app.screen()==Screen::playing);
        CHECK(digest(app)==hash&&app.game()->phase()==phase&&app.controls_armed());
    }
    // Deterministic terminal board: game ends during a real six-tick batch.
    study_grid::Grid tall;for(int c=1;c<10;++c)CHECK(tall.set(4,c,study_grid::Cell::filled));
    auto fast=study_round::Round::create(tall,study_catalog::Kind::T,1);CHECK(fast);
    Application end(*fast);CHECK(end.advance(0,{},true,false,false));
    auto ending=end.advance(.1,{},false,false,true);CHECK(ending&&ending->frame&&end.screen()==Screen::finished);
    const auto ended_hash=digest(end);
    const auto phase=end.game()->phase(),accent=end.game()->accent_state();
    for(int i=0;i<50;++i){auto frozen=end.advance(.1,{},false,false,true);CHECK(frozen&&!frozen->frame);}
    CHECK(digest(end)==ended_hash&&end.game()->phase()==phase&&end.game()->accent_state()==accent);
    auto again=end.advance(.1,{},true,false,false);CHECK(again&&again->game_replaced&&!again->frame);
    CHECK(digest(end)==study_hash::state_hash(*fast)&&end.game()->phase()==0);
    Application terminal(app.game()->round());
    CHECK(terminal.advance(0,{},false,true,true));CHECK(terminal.screen()==Screen::quitting);
    for(int i=0;i<3;++i){CHECK(terminal.advance(.1,{},true,true,true));CHECK(!terminal.game()&&terminal.screen()==Screen::quitting);}
    study_game::Game finish_source(*fast);CHECK(finish_source.advance(.1,{}));
    Application blocked(finish_source.round());auto created=blocked.advance(0,{},true,false,false);
    CHECK(created&&created->game_replaced&&blocked.screen()==Screen::finished);
    auto repeated=blocked.advance(0,{},true,false,false);CHECK(repeated&&repeated->game_replaced&&!repeated->screen_changed);
    // App delegates unchanged gameplay once keys have been released.
    for(std::uint64_t seed=0;seed<50;++seed){
        auto round=study_round::Round::create_seeded(study_grid::Grid{},seed);CHECK(round);
        Application a(*round);study_game::Game direct(*round);
        CHECK(a.advance(0,{},true,false,false));CHECK(a.advance(0,{},false,false,true));
        for(unsigned i=0;i<120&&a.screen()==Screen::playing;++i){
            const double dt=i%2?.017:.1;
            study_loop::FrameInput input{i%5==0,i%7==0,i%11==0,i%3==0,i%13==0,false};
            auto got=a.advance(dt,input,input.drop,false,false);
            const auto ref=direct.advance(dt,input);CHECK(got&&got->frame&&ref);
            CHECK(got->frame->ticks==ref->ticks&&digest(a)==study_hash::state_hash(direct.round()));
            CHECK(a.game()->phase()==direct.phase()&&a.game()->accent_state()==direct.accent_state());
        }
    }
    std::puts("12 transitions, ownership, restart, input gates, frozen finish and 50-seed gameplay parity passed");
}
