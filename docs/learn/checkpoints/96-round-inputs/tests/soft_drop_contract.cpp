#include "simulation/round.h"
#include "simulation/pending_controls.h"
#include "timing/fixed_clock.h"
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <cstdint>
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"soft drop line %d: %s\n",__LINE__,#x);std::exit(1);}}while(false)
static bool piece_equal(const study_piece::Piece& a,const study_piece::Piece& b){
    if(a.origin.row!=b.origin.row||a.origin.column!=b.origin.column)return false;
    for(unsigned i=0;i<4;++i)if(a.local[i].row!=b.local[i].row||a.local[i].column!=b.local[i].column)return false;
    return true;
}
static void same(const study_round::Round& a,const study_round::Round& b){
    CHECK(a.board().cells()==b.board().cells()&&bool(a.active())==bool(b.active()));
    if(a.active())CHECK(piece_equal(*a.active(),*b.active()));
    CHECK(a.soft_drop().remaining==b.soft_drop().remaining&&a.soft_drop().period==b.soft_drop().period);
    CHECK(a.gravity().elapsed==b.gravity().elapsed&&a.gravity().interval==b.gravity().interval);
    CHECK(a.score()==b.score()&&a.total_lines()==b.total_lines()&&a.level()==b.level());
    CHECK(a.last_awarded()==b.last_awarded()&&a.last_cleared()==b.last_cleared());
    CHECK(a.quarter()==b.quarter()&&a.last_rotation_candidate()==b.last_rotation_candidate());
    CHECK(a.kind()==b.kind()&&a.end_reason()==b.end_reason()&&a.source_cursor()==b.source_cursor());
    for(std::size_t i=0;i<3;++i)CHECK(a.next().peek(i)==b.next().peek(i));
}
static study_round::Round batched(std::uint64_t ns,int frames){
    auto round=*study_round::Round::create({},study_catalog::Kind::I,30);
    study_input::PendingControls inputs;study_timing::FixedClock clock;
    for(int frame=0;frame<frames;++frame){
        inputs.capture(false,false,false,true);
        const auto batch=clock.advance_ns(ns);CHECK(!batch.clamped);
        for(unsigned i=0;i<batch.ticks;++i){auto in=inputs.consume();CHECK(in.soft_drop);CHECK(round.tick(in.horizontal,in.clockwise,in.soft_drop)!=study_round::Step::invalid);}
    }
    return round;
}
int main(){
    using R=study_soft_drop::Result;
    unsigned transitions=0;
    for(int period=1;period<=9;++period)for(unsigned mask=0;mask<4096;++mask){
        study_soft_drop::Counter c{0,period};std::int64_t next=0;
        for(int tick=0;tick<12;++tick){
            bool held=(mask&(1u<<tick))!=0;bool due=held&&tick>=next;
            if(!held)next=tick+1;else if(due)next=static_cast<std::int64_t>(tick)+period;
            const auto r=study_soft_drop::tick(held,c);
            CHECK(r==(!held?R::idle:due?R::due:R::waiting));
            CHECK(c.remaining==(!held?0:next-tick-1));++transitions;
        }
    }
    for(const study_soft_drop::Counter bad:{study_soft_drop::Counter{0,0},{0,-1},{-1,4},{4,4},{std::numeric_limits<int>::max(),4}}){
        for(bool held:{false,true}){auto c=bad;CHECK(study_soft_drop::tick(held,c)==R::invalid);CHECK(c.remaining==bad.remaining&&c.period==bad.period);}
    }
    study_soft_drop::Counter huge{0,std::numeric_limits<int>::max()};
    CHECK(study_soft_drop::tick(true,huge)==R::due&&huge.remaining==std::numeric_limits<int>::max()-1);
    CHECK(study_soft_drop::tick(true,huge)==R::waiting&&huge.remaining==std::numeric_limits<int>::max()-2);
    CHECK(study_soft_drop::tick(false,huge)==R::idle&&huge.remaining==0);
    // All 12-tick held histories; independent soft deadlines + gravity multiples.
    for(int interval:{1,2,3,4,6,30,std::numeric_limits<int>::max()})for(unsigned mask=0;mask<4096;++mask){
        auto round=*study_round::Round::create({},study_catalog::Kind::I,interval);
        int row=0,next=1;
        for(int tick=1;tick<=12;++tick){
            const bool held=(mask&(1u<<(tick-1)))!=0;bool soft=held&&tick>=next;
            if(!held)next=tick+1;else if(soft)next=tick+4;
            row+=soft;row+=tick%interval==0;
            // 12 ticks can move at most 24 rows when release/repress alternates;
            // stop this algebraic (no lock) check before the I reaches its last valid origin.
            if(row>18)break;
            const auto step=round.tick(0,false,held);CHECK(step!=study_round::Step::invalid);
            CHECK(round.active()&&round.active()->origin.row==row);
            CHECK(round.gravity().elapsed==tick%interval);
            CHECK(round.soft_drop().remaining==(!held?0:next-tick-1));
        }
    }
    // Held input persists through multiple consumes, edges do not.
    study_input::PendingControls input;input.capture(true,false,true,true);
    auto intent=input.consume();CHECK(intent.horizontal==-1&&intent.clockwise&&intent.soft_drop);
    for(int n=0;n<6;++n){intent=input.consume();CHECK(intent.horizontal==0&&!intent.clockwise&&intent.soft_drop);}
    input.capture(false,false,false,false);CHECK(!input.consume().soft_drop);
    input.capture(false,false,true,true);input.capture(false,false,false,false);
    intent=input.consume();CHECK(intent.clockwise&&!intent.soft_drop); // down sample overwritten before any tick
    same(batched(10'000'000,100),batched(100'000'000,10)); // 60 identical held ticks, including zero-tick frames
    // A soft-blocked lock ends this tick's actions, including gravity on the new piece.
    study_grid::Grid board;for(int col=0;col<10;++col)if(col<3||col>6)CHECK(board.set(1,col,study_grid::Cell::filled));
    CHECK(board.set(2,3,study_grid::Cell::filled));
    auto clear=*study_round::Round::create(board,study_catalog::Kind::I,1);
    CHECK(clear.tick(0,false,true)==study_round::Step::locked);
    CHECK(clear.active()&&clear.active()->origin.row==0&&clear.gravity().elapsed==0);
    CHECK(clear.score()==100&&clear.last_cleared()==1&&clear.soft_drop().remaining==3);
    const auto copy=clear;CHECK(clear.tick(2,false,true)==study_round::Step::invalid);same(copy,clear);
    for(int n=0;n<20;++n){CHECK(clear.ghost());same(copy,clear);}
    // A regular downward success plus a due gravity step may move two rows.
    auto two=*study_round::Round::create({},study_catalog::Kind::I,1);
    CHECK(two.tick(0,false,true)==study_round::Step::changed&&two.active()->origin.row==2);
    // Same tick rotation applies to the current piece before soft drop in this checkpoint.
    auto rotated=*study_round::Round::create({},study_catalog::Kind::I,30);
    CHECK(rotated.tick(0,true,true)==study_round::Step::changed&&rotated.quarter()==1&&rotated.active()->origin.row==1);
    study_grid::Grid stopped_board;CHECK(stopped_board.set(0,4,study_grid::Cell::filled));
    auto stopped=*study_round::Round::create(stopped_board,study_catalog::Kind::T);auto stopped_copy=stopped;
    CHECK(stopped.tick(0,false,true)==study_round::Step::stopped);same(stopped,stopped_copy);
    std::printf("soft drop: %u timing transitions, held histories, invalid/bounds, frame batching, one-lock boundary and read-only queries passed\n",transitions);
}
