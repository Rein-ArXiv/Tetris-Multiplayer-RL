#include "simulation/next_queue.h"
#include "simulation/scripted_source.h"
#include "simulation/round.h"
#include "src/clear_example.h"
#include "renderer/next_preview.h"
#include <deque>
#include <limits>
#include <cstdio>
#include <cstdlib>
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"queue line %d: %s\n",__LINE__,#x);std::exit(1);}}while(false)
using Kind=study_catalog::Kind;
static void compare(const study_next::Queue& queue,const std::deque<Kind>& expected){
    CHECK(queue.size()==expected.size()&&queue.empty()==expected.empty()&&queue.full()==(expected.size()==3));
    for(std::size_t i=0;i<3;++i)CHECK(queue.peek(i)==(i<expected.size()?std::optional<Kind>{expected[i]}:std::nullopt));
    CHECK(!queue.peek(std::numeric_limits<std::size_t>::max()));
}
static bool same(const study_round::Round& a,const study_round::Round& b){
    if(a.board().cells()!=b.board().cells()||a.kind()!=b.kind()||a.quarter()!=b.quarter()||
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
    study_next::Queue queue;std::deque<Kind> expected;
    unsigned state=12345;
    for(unsigned n=0;n<250000;++n){
        state=state*1664525u+1013904223u;
        unsigned operation=(state>>16)%5;
        const auto kind=study_catalog::definitions[(state>>20)%7].kind;
        if(operation<2){bool ok=expected.size()<3;CHECK(queue.push(kind)==ok);if(ok)expected.push_back(kind);}
        else if(operation==2){auto item=queue.pop();CHECK(item==(expected.empty()?std::nullopt:std::optional<Kind>{expected.front()}));if(!expected.empty())expected.pop_front();}
        else if(operation==3)CHECK(!queue.push(static_cast<Kind>(0)));
        else{auto copy=queue;auto view=queue.peek();(void)copy.pop();CHECK(queue.peek()==view);}
        compare(queue,expected);
    }
    // Full capacity means three live entries; no sentinel slot is lost.
    queue={};for(auto kind:{Kind::I,Kind::O,Kind::T})CHECK(queue.push(kind));
    CHECK(queue.full()&&!queue.push(Kind::Z));auto saved=queue.peek();CHECK(queue.pop()==Kind::I);
    CHECK(queue.push(Kind::Z));CHECK(saved==Kind::I);compare(queue,{Kind::O,Kind::T,Kind::Z});
    auto mesh=study_next_view::make_mesh(queue);CHECK(mesh&&mesh->count==72);
    for(const auto& vertex:mesh->vertices)CHECK(vertex.x>=-1&&vertex.x<=1&&vertex.y>=-1&&vertex.y<=1);
    const auto immutable=queue;for(int i=0;i<100;++i)CHECK(study_next_view::make_mesh(queue)->count==72);
    for(int i=0;i<3;++i)CHECK(queue.peek(i)==immutable.peek(i));
    queue={};CHECK(study_next_view::make_mesh(queue)->count==0);
    for(unsigned n=1;n<=3;++n){CHECK(queue.push(Kind::I));CHECK(study_next_view::make_mesh(queue)->count==24*n);}
    // Producer owns pattern data and copies preserve independent cursors.
    std::array<Kind,7> pattern{};pattern[0]=Kind::I;pattern[1]=Kind::O;
    CHECK(!study_next::ScriptedSource::from_pattern(pattern,0));CHECK(!study_next::ScriptedSource::from_pattern(pattern,8));
    CHECK(!study_next::ScriptedSource::from_pattern(pattern,3));
    auto producer=*study_next::ScriptedSource::from_pattern(pattern,2);pattern[0]=Kind::Z;
    CHECK(producer.next()==Kind::I);auto producer_copy=producer;CHECK(producer.next()==Kind::O&&producer_copy.next()==Kind::O);
    CHECK(producer.next()==Kind::I);CHECK(!study_next::ScriptedSource::cycle(static_cast<Kind>(0)));
    study_grid::Grid empty;
    for(int start=0;start<7;++start){
        auto source=*study_next::ScriptedSource::cycle(study_catalog::definitions[start].kind);
        auto round=*study_round::Round::create(empty,source,1);
        CHECK(source.cursor()==0);CHECK(round.source_cursor()==4);
        CHECK(round.kind()==study_catalog::definitions[start].kind);
        for(int i=0;i<3;++i)CHECK(round.next().peek(i)==study_catalog::definitions[(start+i+1)%7].kind);
        auto copy=round;const auto before=round;
        for(int i=0;i<50;++i){
            auto result=round.tick(0);if(result==study_round::Step::locked)break;
            CHECK(result==study_round::Step::changed);
        }
        CHECK(round.kind()==study_catalog::definitions[(start+1)%7].kind&&round.quarter()==0);
        CHECK(round.source_cursor()==5&&round.gravity().elapsed==0);
        for(int i=0;i<3;++i)CHECK(round.next().peek(i)==study_catalog::definitions[(start+i+2)%7].kind);
        CHECK(same(copy,before));const auto changed=round;
        CHECK(round.tick(9,true)==study_round::Step::invalid&&same(round,changed));
    }
    auto source=*study_next::ScriptedSource::cycle(Kind::O);
    auto round=*study_round::Round::create(make_clear_board(Kind::O),source,1);
    for(int i=0;i<18;++i)CHECK(round.tick(0)==study_round::Step::changed);
    CHECK(round.tick(0)==study_round::Step::locked&&round.last_cleared()==2&&round.kind()==Kind::S);
    CHECK(round.next().peek()==Kind::T&&round.source_cursor()==5);
    // Spawn fails after a valid lock: consume/refill once, then stop consuming.
    study_grid::Grid blocked;CHECK(blocked.set(2,4,study_grid::Cell::filled));
    round=*study_round::Round::create(blocked,source,1);
    CHECK(round.tick(0)==study_round::Step::game_over&&!round.active());
    CHECK(round.kind()==Kind::S&&round.next().peek()==Kind::T&&round.source_cursor()==5);
    const auto finished=round;CHECK(round.tick(0)==study_round::Step::stopped&&same(round,finished));
    CHECK(round.board().get(0,4)==study_grid::Cell::filled);
    // Initially blocked placement still has three previews, producer used four values.
    CHECK(blocked.set(0,4,study_grid::Cell::filled));
    round=*study_round::Round::create(blocked,source,1);CHECK(round.finished()&&round.next().full()&&round.source_cursor()==4);
    CHECK(!study_round::Round::create(empty,source,0));CHECK(source.cursor()==0);
    std::puts("250000 FIFO/model operations; all-capacity/wrap/peek/copy, producer ownership, seven promotions, spawn-failure consumption, stopped/invalid preservation and preview counts passed");
}
