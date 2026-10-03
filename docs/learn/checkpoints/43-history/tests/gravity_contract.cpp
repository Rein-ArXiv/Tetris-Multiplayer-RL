#include "simulation/gravity.h"
#include "simulation/catalog.h"
#include "simulation/pending_horizontal.h"
#include "timing/fixed_clock.h"
#include "src/gravity_example.h"
#include <cstdio>
#include <limits>
#include <cstdlib>
#define CHECK(x) do { if (!(x)) { std::fprintf(stderr,"gravity line %d: %s\n",__LINE__,#x); std::exit(1); } } while(false)
static bool same(const study_piece::Piece& a,const study_piece::Piece& b) {
    if(a.origin.row!=b.origin.row||a.origin.column!=b.origin.column)return false;
    for(unsigned i=0;i<4;++i)if(a.local[i].row!=b.local[i].row||a.local[i].column!=b.local[i].column)return false;
    return true;
}
// Independent row-major masks, not classify() or to_board().
static bool clear(int k,int row,int col,int obstacle) {
    const unsigned masks[]={0xF0,0x71,0x74,0x33,0x36,0x72,0x63};
    for(int r=0;r<4;++r)for(int c=0;c<4;++c)if(masks[k]&(1u<<(r*4+c))) {
        int y=row+r,x=col+c;
        if(y<0||y>=20||x<0||x>=10||y*10+x==obstacle)return false;
    }
    return true;
}
int main() {
    using namespace study_gravity;
    unsigned cases=0;
    for(int obstacle=-1;obstacle<200;++obstacle) {
        study_grid::Grid board;
        if(obstacle>=0) CHECK(board.set(obstacle/10,obstacle%10,study_grid::Cell::filled));
        const auto snapshot=board.cells();
        for(int k=0;k<7;++k)for(int row=-1;row<=20;++row)for(int col=-1;col<=10;++col) {
            auto piece=*study_catalog::make_piece(study_catalog::definitions[k].kind);
            piece.origin={row,col};const auto before=piece;
            Counter counter{29,30};const auto result=tick(board,piece,counter);
            const bool valid=clear(k,row,col,obstacle),free_below=clear(k,row+1,col,obstacle);
            if(!valid) { CHECK(result==TickResult::invalid);CHECK(counter.elapsed==29);CHECK(same(piece,before)); }
            else if(!free_below) { CHECK(result==TickResult::blocked);CHECK(counter.elapsed==0);CHECK(same(piece,before)); }
            else { CHECK(result==TickResult::moved);CHECK(counter.elapsed==0);auto expected=before;++expected.origin.row;CHECK(same(piece,expected)); }
            CHECK(counter.interval==30);CHECK(board.cells()==snapshot);++cases;
        }
    }
    auto board=make_gravity_board();auto piece=*study_catalog::make_piece(study_catalog::Kind::T);
    const auto initial=piece; Counter counter;
    for(int t=1;t<=300;++t) {
        auto result=tick(board,piece,counter);
        CHECK(counter.elapsed==t%30);CHECK(piece.origin.row==(t/30<8?t/30:8));
        CHECK(result==(t%30?TickResult::waiting:t<=240?TickResult::moved:TickResult::blocked));
    }
    // No locking: move off the ledge, then resume at the existing cadence.
    CHECK(study_collision::try_shift(board,piece,1)==study_movement::Result::moved);
    CHECK(study_collision::try_shift(board,piece,1)==study_movement::Result::moved);
    counter.elapsed=29;CHECK(tick(board,piece,counter)==TickResult::moved);CHECK(piece.origin.row==9);
    for(auto bad : {Counter{0,0},Counter{-1,30},Counter{30,30},Counter{0,-1}}) {
        piece=initial;const auto before=bad;CHECK(tick(board,piece,bad)==TickResult::invalid);
        CHECK(same(piece,initial));CHECK(bad.elapsed==before.elapsed&&bad.interval==before.interval);
    }
    piece=initial;Counter large{std::numeric_limits<int>::max()-2,std::numeric_limits<int>::max()};
    CHECK(tick(board,piece,large)==TickResult::waiting);CHECK(large.elapsed==std::numeric_limits<int>::max()-1);
    CHECK(tick(board,piece,large)==TickResult::moved);CHECK(large.elapsed==0);
    // A valid rebased piece can still have an unrepresentable next origin.
    piece=initial;const int top=std::numeric_limits<int>::max();piece.origin.row=top;
    for(auto& cell:piece.local)cell.row-=top;
    const auto extreme=piece;counter={29,30};CHECK(tick(board,piece,counter)==TickResult::invalid);
    CHECK(same(piece,extreme)&&counter.elapsed==29);
    // Clock carries phase, clamps before multiply, and rejects invalid doubles atomically.
    using study_timing::FixedClock;
    FixedClock clock;
    CHECK(clock.advance_ns(16'666'666).ticks==0);CHECK(clock.phase()==999'999'960);
    CHECK(clock.advance_ns(1).ticks==1);CHECK(clock.phase()==20);
    auto huge=clock.advance_ns(std::numeric_limits<std::uint64_t>::max());
    CHECK(huge.ticks==6&&huge.clamped&&clock.phase()==20);
    for(double dt:{-1.0,std::numeric_limits<double>::infinity(),std::numeric_limits<double>::quiet_NaN()}) {
        CHECK(!clock.advance_seconds(dt));CHECK(clock.phase()==20);
    }
    CHECK(clock.advance_seconds(1e300)->ticks==6);CHECK(clock.phase()==20);
    CHECK(clock.advance_seconds(0)->ticks==0);CHECK(clock.advance_seconds(0.01)->ticks==0);
    FixedClock a,b;unsigned at=0,bt=0;
    for(int i=0;i<10;++i)at+=a.advance_ns(100'000'000).ticks;
    for(int i=0;i<1000;++i)bt+=b.advance_ns(1'000'000).ticks;
    CHECK(at==60&&bt==60&&a.phase()==0&&b.phase()==0);
    // Deterministic varied partitions of the same accepted integer duration.
    FixedClock varied;std::uint64_t remaining=1'000'000'000;unsigned vt=0,state=5;
    while(remaining) { state=state*1664525u+1013904223u;auto n=std::uint64_t(state%10'000'000+1);if(n>remaining)n=remaining;vt+=varied.advance_ns(n).ticks;remaining-=n; }
    CHECK(vt==60&&varied.phase()==0);
    // One edge survives zero-tick frames, and is not repeated in a six-tick batch.
    study_input::PendingHorizontal pending;piece=initial;counter={0,30};FixedClock input_clock;
    pending.capture(false,true);CHECK(input_clock.advance_ns(1'000'000).ticks==0);
    pending.capture(false,false);auto batch=input_clock.advance_ns(100'000'000);
    for(unsigned i=0;i<batch.ticks;++i) {
        CHECK(study_collision::try_shift(board,piece,pending.consume())!=study_movement::Result::invalid);
        CHECK(tick(board,piece,counter)==TickResult::waiting);
    }
    CHECK(piece.origin.column==initial.origin.column+1&&counter.elapsed==6);
    pending.capture(true,false);pending.capture(false,true);CHECK(pending.consume()==0);
    pending.capture(false,true);pending.capture(false,true);CHECK(pending.consume()==1);CHECK(pending.consume()==0);
    // Same-tick order is observable: left can hit a ledge only before falling.
    study_grid::Grid order_board;CHECK(order_board.set(0,3,study_grid::Cell::filled));
    auto left_first=*study_catalog::make_piece(study_catalog::Kind::T),down_first=left_first;
    Counter ca{29,30},cb=ca;
    CHECK(study_collision::try_shift(order_board,left_first,-1)==study_movement::Result::blocked);
    CHECK(tick(order_board,left_first,ca)==TickResult::moved);
    CHECK(tick(order_board,down_first,cb)==TickResult::moved);
    CHECK(study_collision::try_shift(order_board,down_first,-1)==study_movement::Result::moved);
    CHECK(left_first.origin.column==3&&down_first.origin.column==2);
    std::printf("%u independent gravity transitions; cadence, preservation, integer limits, clock, input and order passed\n",cases);
}
