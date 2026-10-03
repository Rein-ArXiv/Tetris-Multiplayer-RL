#include "simulation/score.h"
#include "simulation/round.h"
#include "src/score_example.h"
#include "src/spawn_example.h"
#include "renderer/score_view.h"
#include <limits>
#include <string>
#include <cstdio>
#include <cstdlib>
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"score line %d: %s\n",__LINE__,#x);std::exit(1);}}while(false)
static bool same(const study_round::Round& a,const study_round::Round& b){
    if(a.score()!=b.score()||a.total_lines()!=b.total_lines()||a.level()!=b.level()||a.last_awarded()!=b.last_awarded()||a.end_reason()!=b.end_reason()||a.next().size()!=b.next().size()||a.board().cells()!=b.board().cells()||a.kind()!=b.kind()||a.quarter()!=b.quarter()||
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


int main(){
    using namespace study_score;using namespace study_round;
    constexpr auto maximum=std::numeric_limits<std::uint64_t>::max();
    const std::uint64_t bases[]={0,100,300,600,1000};
    for(unsigned lev=0;lev<=21;++lev)for(int rows=-1;rows<=5;++rows){
        const auto value=normal_points(rows,lev);const bool valid=lev>=1&&lev<=20&&rows>=0&&rows<=4;
        CHECK(bool(value)==valid);if(valid)CHECK(*value==bases[rows]*lev);
    }
    for(auto total:{0ull,8ull,9ull,10ull,18ull,19ull,188ull,189ull,190ull,191ull})for(int rows=0;rows<=4;++rows){
        const Totals before{123,total};const auto after=award(before,rows);CHECK(after);
        CHECK(after->points==123+bases[rows]*(total>=190?20:1+total/10));
        CHECK(after->lines==total+rows&&before.points==123&&before.lines==total);
    }
    CHECK(!award({},-1)&&!award({},5));
    for(std::uint64_t remaining=0;remaining<=20001;++remaining){
        Totals before{maximum-remaining,maximum-1};auto after=award(before,4);CHECK(after);
        CHECK(after->points==(remaining<20000?maximum:maximum-remaining+20000));
        CHECK(after->lines==maximum&&level(*after)==20);
    }
    CHECK(saturating_add(maximum,0)==maximum&&saturating_add(0,maximum)==maximum);
    CHECK(!gravity_interval(0)&&!gravity_interval(21));
    const int intervals[]={30,29,28,26,25,23,22,21,19,18,16,15,13,12,11,9,8,6,5,3};
    for(unsigned lev=1;lev<=20;++lev)CHECK(gravity_interval(lev)==intervals[lev-1]);
    auto round=*Round::create(make_four_clears(),study_catalog::Kind::I,1);
    const std::uint64_t points[]={1000,2000,3000,5000},gains[]={1000,1000,1000,2000};
    for(int lock=0;lock<4;++lock){
        auto before=round;CHECK(round.tick(9,true)==Step::invalid&&same(round,before));
        auto step=round.tick(0,true);int ticks=1;
        while(step!=Step::locked&&ticks<1000){CHECK(step==Step::changed||step==Step::waiting);step=round.tick(0);++ticks;}
        CHECK(step==Step::locked&&round.last_cleared()==4&&round.last_awarded()==gains[lock]);
        CHECK(round.score()==points[lock]&&round.total_lines()==4u*(lock+1)&&round.level()==(lock<2?1u:2u));
        CHECK(round.gravity().interval==(lock<2?1:29)&&round.gravity().elapsed==0);
        const auto snapshot=round;
        for(int query=0;query<100;++query){(void)study_score_view::make_mesh(round.score());CHECK(same(round,snapshot));}
    }
    const auto points_before=round.score();CHECK(round.tick(0)==Step::waiting&&round.last_awarded()==0&&round.score()==points_before);
    // Starting with an unresolved full row violates the new scoring precondition.
    for(int row=0;row<20;++row){study_grid::Grid full;for(int c=0;c<10;++c)CHECK(full.set(row,c,study_grid::Cell::filled));CHECK(!Round::create(full,study_catalog::Kind::O));}
    for(int k=0;k<7;++k){
        const auto kind=study_catalog::definitions[k].kind;
        auto rescue=*Round::create(*spawn_example::make(kind,spawn_example::Scenario::clear_rescue),kind,1);
        CHECK(rescue.tick(0)==Step::locked&&rescue.score()==(k==0?100u:300u));
    }
    // A valid clear followed by blocked spawn must retain the earned points.
    study_grid::Grid ended_board;
    for(int c=0;c<10;++c)if(c!=4&&c!=5)CHECK(ended_board.set(1,c,study_grid::Cell::filled));
    CHECK(ended_board.set(2,4,study_grid::Cell::filled));
    auto ended=*Round::create(ended_board,study_catalog::Kind::O,1);
    CHECK(ended.tick(0)==Step::game_over&&ended.score()==100&&ended.last_awarded()==100);
    const auto snapshot=ended;for(int i=0;i<100;++i)CHECK(ended.tick(0)==Step::stopped&&same(ended,snapshot));
    // Count segments independently using their letter names, not the mesh's masks.
    const char* segments[]={"abcdef","bc","abdeg","abcdg","bcfg","acdfg","acdefg","abc","abcdefg","abcdfg"};
    for(std::uint64_t value:{std::uint64_t{0},std::uint64_t{1234567890},maximum}){
        const auto mesh=study_score_view::make_mesh(value);std::size_t expected=0;
        for(char digit:std::to_string(value))for(const char* s=segments[digit-'0'];*s;++s)expected+=6;
        CHECK(mesh.count==expected&&mesh.count<=mesh.capacity);
        for(std::size_t i=0;i<mesh.count;++i)CHECK(mesh.vertices[i].x>=-1&&mesh.vertices[i].x<=1&&mesh.vertices[i].y>=-1&&mesh.vertices[i].y<=1);
    }
    std::puts("score table/level thresholds, 20002 saturation boundaries, four real clears, query/invalid/stopped preservation, full-row precondition, final award and digit geometry passed");
}
