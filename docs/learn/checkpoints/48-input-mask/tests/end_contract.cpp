#include "simulation/spawn.h"
#include "simulation/round.h"
#include "src/spawn_example.h"
#include "renderer/end_marker.h"
#include "tests/locking_oracle.h"
#include <limits>
#include <cstdio>
#include <cstdlib>
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"end line %d: %s\n",__LINE__,#x);std::exit(1);}}while(false)
using namespace study_round;
using Kind=study_catalog::Kind;
static bool same(const study_round::Round& a,const study_round::Round& b){
    if(a.end_reason()!=b.end_reason()||a.next().size()!=b.next().size()||a.board().cells()!=b.board().cells()||a.kind()!=b.kind()||a.quarter()!=b.quarter()||
       a.source_cursor()!=b.source_cursor()||a.last_cleared()!=b.last_cleared()||
       a.last_rotation_candidate()!=b.last_rotation_candidate()||a.gravity().elapsed!=b.gravity().elapsed||
       a.gravity().interval!=b.gravity().interval||bool(a.active())!=bool(b.active()))return false;
    for(unsigned i=0;i<3;++i)if(a.next().peek(i)!=b.next().peek(i))return false;
    if(a.active()){
        if(a.active()->origin.row!=b.active()->origin.row||a.active()->origin.column!=b.active()->origin.column)return false;
        for(unsigned i=0;i<4;++i)if(a.active()->local[i].row!=b.active()->local[i].row||a.active()->local[i].column!=b.active()->local[i].column)return false;
    }
    return true;
}

static void frozen(Round round){
    const auto before=round;
    for(int i=0;i<100;++i){CHECK(round.tick(i%5-2,i%2)==Step::stopped);CHECK(same(before,round));}
}
int main(){
    using Status=study_spawn::Status;
    study_grid::Grid empty;
    auto piece=*study_catalog::make_piece(Kind::O);
    CHECK(study_spawn::assess(empty,piece)==Status::ready);
    piece.origin.column=-1;CHECK(study_spawn::assess(empty,piece)==Status::blocked);
    piece.origin.column=std::numeric_limits<int>::max();
    CHECK(study_spawn::assess(empty,piece)==Status::invalid);
    for(int k=0;k<7;++k){
        const Kind kind=study_catalog::definitions[k].kind;
        const auto source=*study_next::ScriptedSource::cycle(kind);
        for(int i=0;i<200;++i){
            study_grid::Grid board;CHECK(board.set(i/10,i%10,study_grid::Cell::filled));
            locking_oracle::Board expected{};expected[i]=true;
            const bool blocked=!locking_oracle::fits(expected,k,0,k==3?4:3);
            const auto round=Round::create(board,source,1);CHECK(round);
            CHECK(round->finished()==blocked&&bool(round->active())==!blocked);
            CHECK(round->end_reason()==(blocked?EndReason::initial_spawn_blocked:EndReason::none));
            CHECK(round->board().cells()==board.cells()&&round->next().size()==3&&round->source_cursor()==4);
            if(blocked)frozen(*round);
        }
        for(auto scenario:{spawn_example::Scenario::initial_blocked,spawn_example::Scenario::next_blocked,spawn_example::Scenario::clear_rescue}){
            const auto board=spawn_example::make(kind,scenario);CHECK(board);
            auto round=*Round::create(*board,source,1);const auto before=round;
            if(scenario==spawn_example::Scenario::initial_blocked){CHECK(round.end_reason()==EndReason::initial_spawn_blocked);frozen(round);continue;}
            CHECK(!round.finished()&&round.active());
            CHECK(round.tick(9,true)==Step::invalid&&same(round,before));
            const auto result=round.tick(0);
            const bool rescue=scenario==spawn_example::Scenario::clear_rescue;
            CHECK(result==(rescue?Step::locked:Step::game_over));
            CHECK(round.end_reason()==(rescue?EndReason::none:EndReason::spawn_blocked));
            CHECK(round.finished()!=bool(round.active()));
            CHECK(round.last_cleared()==(rescue?(k==0?1:2):0));
            CHECK(round.kind()==study_catalog::definitions[(k+1)%7].kind&&round.source_cursor()==5);
            CHECK(round.next().size()==3&&round.gravity().elapsed==0&&round.quarter()==0);
            for(int slot=0;slot<3;++slot)CHECK(round.next().peek(slot)==study_catalog::definitions[(k+slot+2)%7].kind);
            // Independent board filter: fill original spawn mask, discard full rows,
            // pack surviving rows from the bottom into a fresh scratch board.
            locking_oracle::Board expected{},filtered{};
            for(int i=0;i<200;++i)expected[i]=board->cells()[i]==study_grid::Cell::filled;
            locking_oracle::fill(expected,k,0,k==3?4:3);
            int write=19;
            for(int row=19;row>=0;--row){
                bool full=true;for(int col=0;col<10;++col)full=full&&expected[row*10+col];
                if(full)continue;
                for(int col=0;col<10;++col)filtered[write*10+col]=expected[row*10+col];
                --write;
            }
            for(int i=0;i<200;++i)CHECK((round.board().cells()[i]==study_grid::Cell::filled)==filtered[i]);
            CHECK(same(before,*Round::create(*board,source,1))); // copy is independent
            if(!rescue)frozen(round);
        }
        auto normal=*Round::create(empty,source,1);
        for(int i=0;i<18;++i)CHECK(normal.tick(0)==Step::changed);
        CHECK(normal.tick(0)==Step::locked&&!normal.finished());
    }
    const auto marker=study_end_view::make_mesh();CHECK(marker.size()==12);
    for(auto vertex:marker)CHECK(vertex.x>=-1&&vertex.x<=1&&vertex.y>=-1&&vertex.y<=1);
    CHECK(!Round::create(empty,Kind::T,0));
    std::puts("1400 single-obstacle spawns, seven end/rescue scenarios, independent board filter, absorbing state and copy preservation passed");
}
