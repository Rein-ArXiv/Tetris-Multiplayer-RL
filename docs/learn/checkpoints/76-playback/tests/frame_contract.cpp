#include "loop/frame_runner.h"
#include "src/spin_example.h"
#include "renderer/board_geometry.h"
#include <cstdio>
#include <cstdlib>
#include <limits>
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"frame line %d: %s\n",__LINE__,#x);std::exit(1);}}while(false)
using study_round::Round;using study_round::Step;using study_catalog::Kind;
static void same(const Round& a,const Round& b) {
    CHECK(a.board().cells()==b.board().cells()&&bool(a.active())==bool(b.active()));
    if(a.active()) {
        CHECK(a.active()->origin.row==b.active()->origin.row&&a.active()->origin.column==b.active()->origin.column);
        CHECK(study_rotation::same_cells(a.active()->local,b.active()->local));
    }
    CHECK(a.rotation_ready()==b.rotation_ready()&&a.clear_streak()==b.clear_streak()&&a.last_t_spin_lines()==b.last_t_spin_lines());
    CHECK(a.score()==b.score()&&a.last_awarded()==b.last_awarded()&&a.total_lines()==b.total_lines());
    CHECK(a.attack_sent()==b.attack_sent()&&a.pending_garbage()==b.pending_garbage()&&a.last_garbage()==b.last_garbage());
    CHECK(a.quarter()==b.quarter()&&a.kind()==b.kind()&&a.end_reason()==b.end_reason());
    CHECK(a.gravity().elapsed==b.gravity().elapsed&&a.gravity().interval==b.gravity().interval);
    CHECK(a.soft_drop().remaining==b.soft_drop().remaining&&a.soft_drop().period==b.soft_drop().period);
    CHECK(a.last_hard_drop_distance()==b.last_hard_drop_distance()&&a.last_rotation_candidate()==b.last_rotation_candidate());
    CHECK(a.last_cleared()==b.last_cleared()&&a.hole_cursor()==b.hole_cursor()&&a.source_cursor()==b.source_cursor());
    for(unsigned i=0;i<3;++i)CHECK(a.next().peek(i)==b.next().peek(i));
}
int main() {
    using study_loop::FrameRunner;using study_loop::FrameInput;
    auto start=*Round::create(make_spin_board(),Kind::T);
    FrameRunner runner(start);
    auto a=runner.advance(.008,{false,false,true,false,true});CHECK(a&&a->ticks==0&&!a->board_changed);same(runner.round(),start);
    auto clone=runner;
    for(double dt:{-1.0,std::numeric_limits<double>::infinity(),std::numeric_limits<double>::quiet_NaN()}) {
        CHECK(!runner.advance(dt,{true,false,true,true,true}));same(runner.round(),clone.round());CHECK(runner.phase()==clone.phase());
    }
    auto b=runner.advance(.009,{});auto cb=clone.advance(.009,{});CHECK(b&&cb&&b->ticks==1);same(runner.round(),clone.round());
    CHECK(b->observations[0].lock&&b->observations[0].lock->spin_lines==1&&b->observations[0].lock->points==800);
    CHECK(b->board_changed&&b->piece_changed&&b->cleared==1);
    const auto saved=*b;
    auto c=runner.advance(.05,{});CHECK(c&&c->ticks==3&&!c->board_changed&&!c->piece_changed);
    CHECK(runner.round().last_t_spin_lines()==-1&&saved.observations[0].lock->spin_lines==1);
    CHECK(saved.observations[0].lock->points==800);
    // An early event followed by waiting ticks must keep dirty flags and snapshot.
    FrameRunner batch(start);auto report=batch.advance(.1,{false,false,true,false,true});CHECK(report&&report->ticks==6&&report->board_changed&&report->piece_changed);
    CHECK(report->observations[0].lock&&report->observations[0].lock->spin_lines==1);
    for(unsigned i=1;i<6;++i)CHECK(!report->observations[i].lock&&report->observations[i].step==Step::waiting);
    CHECK(batch.round().last_t_spin_lines()==-1);
    // Three locks in one batch, ending before the final scheduled tick.
    study_grid::Grid support;CHECK(support.set(6,4,study_grid::Cell::filled));CHECK(support.set(6,5,study_grid::Cell::filled));
    auto top=*Round::create(support,Kind::O,1);for(int i=0;i<4;++i)CHECK(top.tick(0)==Step::changed);
    FrameRunner many(top);auto m=many.advance(.1,{false,false,false,false,true});CHECK(m&&m->ticks==6);
    const Step expected[]={Step::locked,Step::changed,Step::changed,Step::locked,Step::game_over,Step::stopped};
    unsigned locks=0;
    for(unsigned i=0;i<6;++i){CHECK(m->observations[i].step==expected[i]);locks+=bool(m->observations[i].lock);}
    CHECK(locks==3&&m->observations[0].lock->hard_drop_distance==0&&m->observations[3].lock->hard_drop_distance==-1);
    CHECK(m->observations[4].lock->end_reason==study_round::EndReason::spawn_blocked);
    auto stopped=many.advance(.1,{false,false,true,true,true});CHECK(stopped&&stopped->ticks==6&&!stopped->board_changed&&!stopped->piece_changed);
    for(auto observation:stopped->observations)CHECK(observation.step==Step::stopped&&observation.kick==-1&&!observation.lock);
    // Same 60 empty-input ticks, different frame partition. No clamp or input
    // sample difference: 50x20ms and10x100ms both schedule exactly 60 ticks.
    FrameRunner fast(start),slow(start);unsigned ft=0,st=0;
    for(int i=0;i<50;++i){auto r=fast.advance(.02,{});CHECK(r);ft+=r->ticks;}
    for(int i=0;i<10;++i){auto r=slow.advance(.1,{});CHECK(r);st+=r->ticks;}
    CHECK(ft==60&&st==60);same(fast.round(),slow.round());CHECK(fast.phase()==slow.phase());
    auto before=fast.round();for(int i=0;i<100;++i){(void)study_board::make_mesh(fast.round().board());(void)fast.round().ghost();}same(before,fast.round());
    FrameRunner clamp(start);auto big=clamp.advance(std::numeric_limits<double>::max(),{});CHECK(big&&big->ticks==6&&big->clamped);
    // Held Down is replaced in zero-tick frames, while Up/Drop edges are latched.
    FrameRunner held(start);CHECK(held.advance(0,{false,false,false,true,false}));CHECK(held.advance(0,{}));
    auto none=held.advance(.02,{});CHECK(none&&held.round().active()->origin.row==0);
    auto down=held.advance(.02,{false,false,false,true,false});CHECK(down&&held.round().active()->origin.row==1);
    // Cancellation is part of the transactional frame, including zero-tick frames.
    auto empty=*Round::create({},Kind::T);
    FrameRunner pending(empty);CHECK(pending.advance(0,{true,false,true,true,true}));
    auto cancelled=pending;
    CHECK(!pending.advance(-1,{false,false,false,false,false,true}));
    auto preserved=pending.advance(.017,{});CHECK(preserved&&preserved->observations[0].lock);
    CHECK(cancelled.advance(0,{false,false,false,false,false,true}));
    auto clean=cancelled.advance(.017,{});CHECK(clean&&!clean->board_changed);
    FrameRunner baseline(empty);CHECK(baseline.advance(.017,{}));same(cancelled.round(),baseline.round());
    FrameRunner fresh(empty),right_only(empty);
    CHECK(fresh.advance(0,{true,false,true,true,true}));
    CHECK(fresh.advance(.017,{false,true,false,false,false,true}));
    CHECK(right_only.advance(.017,{false,true,false,false,false}));same(fresh.round(),right_only.round());
    std::puts("Frame snapshots, 0/1/6 ticks, three locks, stopped suppression, invalid rollback, input lifetimes, 60-tick partitions and read-only projection passed");
}
