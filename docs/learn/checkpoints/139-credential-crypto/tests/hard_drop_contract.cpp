#include "simulation/round.h"
#include "simulation/pending_controls.h"
#include "tests/locking_oracle.h"
#include "src/spawn_example.h"
#include <cstdio>
#include <cstdlib>
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"hard drop line %d: %s\n",__LINE__,#x);std::exit(1);}}while(false)
using namespace study_round;
using Kind=study_catalog::Kind;
static bool piece_equal(const study_piece::Piece& a,const study_piece::Piece& b){
    if(a.origin.row!=b.origin.row||a.origin.column!=b.origin.column)return false;
    for(unsigned i=0;i<4;++i)if(a.local[i].row!=b.local[i].row||a.local[i].column!=b.local[i].column)return false;
    return true;
}
static void same(const study_round::Round& a,const study_round::Round& b){
    CHECK(a.last_hard_drop_distance()==b.last_hard_drop_distance());
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

// Plain cell offsets and a fresh destination board; no ghost/locking/line helpers.
static bool fits(const locking_oracle::Board& b,const study_piece::Piece& p,int delta) {
    for(auto cell:p.local) {
        const int r=cell.row+p.origin.row+delta,c=cell.column+p.origin.column;
        if(r<0||r>=20||c<0||c>=10||b[r*10+c])return false;
    }
    return true;
}
static void check_drop(Round& round,bool down=false) {
    CHECK(round.active());const auto before=round;
    locking_oracle::Board board{};
    for(int n=0;n<200;++n)board[n]=round.board().cells()[n]==study_grid::Cell::filled;
    const auto piece=*round.active();CHECK(fits(board,piece,0));
    int distance=0;while(fits(board,piece,distance+1)){++distance;CHECK(distance<20);}
    for(auto cell:piece.local)board[(cell.row+piece.origin.row+distance)*10+cell.column+piece.origin.column]=true;
    locking_oracle::Board filtered{};int write=19,cleared=0;
    for(int row=19;row>=0;--row) {
        bool full=true;for(int col=0;col<10;++col)full=full&&board[row*10+col];
        if(full){++cleared;continue;}
        for(int col=0;col<10;++col)filtered[write*10+col]=board[row*10+col];
        --write;
    }
    const auto next=*round.next().peek();const auto spawn=*study_catalog::make_piece(next);
    const bool ready=fits(filtered,spawn,0);
    const auto result=round.tick(0,false,down,true);
    CHECK(result==(ready?Step::locked:Step::game_over));
    CHECK(round.last_hard_drop_distance()==distance&&round.last_cleared()==cleared);
    CHECK(round.source_cursor()==(before.source_cursor()+1)%7); // ScriptedSource cycles seven kinds.
    CHECK(round.kind()==next&&round.quarter()==0&&round.gravity().elapsed==0);
    for(unsigned i=0;i<2;++i)CHECK(round.next().peek(i)==before.next().peek(i+1));
    for(int n=0;n<200;++n)CHECK((round.board().cells()[n]==study_grid::Cell::filled)==filtered[n]);
    const unsigned scores[]={0,100,300,500,1000};
    CHECK(cleared>=0&&cleared<=4);
    CHECK(round.last_awarded()==scores[cleared]*before.level());
    CHECK(round.score()==before.score()+round.last_awarded());
    CHECK(round.total_lines()==before.total_lines()+cleared);
    const int wait=before.soft_drop().remaining;
    CHECK(round.soft_drop().remaining==(down?(wait?wait-1:3):0));
    if(ready)CHECK(piece_equal(*round.active(),spawn));
    const auto published=round;(void)round.ghost();same(round,published);
}
int main() {
    study_input::PendingControls pending;
    pending.capture(false,false,false,true,true);
    pending.capture(false,false,false,true,false); // zero-tick frame, request survives
    for(int n=0;n<3;++n){const auto in=pending.consume();CHECK(in.hard_drop==(n==0));CHECK(in.soft_drop);}
    pending.capture(false,false,false,false,true);
    pending.capture(false,false,false,false,true); // coalesced, not counted
    CHECK(pending.consume().hard_drop);CHECK(!pending.consume().hard_drop);
    unsigned cases=0;
    for(int kind=0;kind<7;++kind)for(int obstacle=-1;obstacle<200;++obstacle)
    for(int interval:{1,30})for(bool held:{false,true}) {
        study_grid::Grid board;if(obstacle>=0)CHECK(board.set(obstacle/10,obstacle%10,study_grid::Cell::filled));
        const auto source=*study_next::ScriptedSource::cycle(study_catalog::definitions[kind].kind);
        auto round=*Round::create(board,source,interval);const auto original=round;
        if(round.finished()) {CHECK(round.tick(2,true,true,true)==Step::stopped);same(round,original);continue;}
        CHECK(round.tick(2,true,true,true)==Step::invalid);same(round,original);
        check_drop(round,held);++cases;
    }
    for(const auto& def:study_catalog::definitions) {
        const auto source=*study_next::ScriptedSource::cycle(def.kind);
        for(auto mode:{spawn_example::Scenario::next_blocked,spawn_example::Scenario::clear_rescue}) {
            auto round=*Round::create(*spawn_example::make(def.kind,mode),source,1);
            check_drop(round,true);CHECK(round.last_hard_drop_distance()==0);
            if(round.finished()){const auto before=round;CHECK(round.tick(0,false,false,true)==Step::stopped);same(round,before);}
        }
        // Repeated commands are not idempotent; exactly one spawn per accepted command.
        auto stack=*Round::create({},source,30);
        for(int n=0;n<20&&!stack.finished();++n)check_drop(stack,n%2);
        CHECK(stack.finished());
        // Isolate the landing algorithm from rotation: inspect the accepted shape,
        // then use a plain-cell oracle for each orientation and horizontal shift.
        for(int turns=0;turns<4;++turns)for(int shift=-2;shift<=2;++shift) {
            auto round=*Round::create({},source,100);
            for(int n=0;n<turns;++n)CHECK(round.tick(0,true)==Step::changed);
            for(int n=0;n<std::abs(shift);++n)CHECK(round.tick(shift<0?-1:1)==Step::changed);
            check_drop(round,true);
        }
    }
    auto batch=*Round::create({},Kind::I,30);study_input::PendingControls one;
    one.capture(false,false,false,false,true);
    for(int n=0;n<3;++n) {
        const auto in=one.consume();auto s=batch.tick(in.horizontal,in.clockwise,in.soft_drop,in.hard_drop);
        CHECK(s==(n==0?Step::locked:Step::waiting));
        CHECK(batch.last_hard_drop_distance()==(n==0?18:-1));
    }
    unsigned filled=0;for(auto c:batch.board().cells())filled+=c==study_grid::Cell::filled;CHECK(filled==4);
    // Movement+rotation belong to the piece being dropped. At spawn I rotates
    // about the 4x4 pivot to local column2, then settles at rows16..19.
    auto compound=*Round::create({},Kind::I,1);
    CHECK(compound.tick(1,true,true,true)==Step::locked);
    CHECK(compound.last_hard_drop_distance()==16&&compound.active()->origin.row==0);
    for(int r=0;r<20;++r)for(int c=0;c<10;++c)
        CHECK((compound.board().get(r,c)==study_grid::Cell::filled)==(c==6&&r>=16));
    CHECK(compound.soft_drop().remaining==3&&compound.gravity().elapsed==0);
    std::printf("%u single-obstacle/clock cases, independent land-clear-spawn, zero distance, compound ordering, one-shot latch and rotated stacks passed\n",cases);
}
