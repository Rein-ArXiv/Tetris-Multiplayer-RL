#include "client/menu_model.h"
#include "client/application.h"
#include "simulation/state_hash.h"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <limits>
#define CHECK(e) do{if(!(e)){std::fprintf(stderr,"CHECK %d: %s\n",__LINE__,#e);std::exit(1);}}while(false)
using study_menu::Action;
using study_menu::Focus;
static study_ui::Input press(double x,double y){return {{study_ui::Point{x,y}},{study_ui::Point{x,y}},false,false};}
int main(){
    const study_ui::Rect box{0,0,100,20};
    const auto hit=press(10,10);
    const auto checked=study_ui::checkbox(box,hit,true);CHECK(checked.checked&&checked.toggle_requested);
    CHECK(study_ui::checkbox(box,hit,false).toggle_requested&&!study_ui::checkbox(box,hit,false).checked);
    CHECK(!study_ui::checkbox(box,hit,true,false).toggle_requested);
    for(std::size_t count:{std::size_t(1),std::size_t(2),std::size_t(100),(std::numeric_limits<std::size_t>::max)()}) {
        for(std::size_t index:{std::size_t(0),count-1}) {
            const auto left=study_ui::selector(box,press(0,10),index,count);
            const auto right=study_ui::selector(box,press(99,10),index,count);
            CHECK(left.previous_enabled==(index>0)&&right.next_enabled==(index<count-1));
            CHECK(left.direction==(index>0?-1:0)&&right.direction==(index<count-1?1:0));
        }
    }
    CHECK(study_ui::selector(box,press(50,10),0,2).direction==0);
    CHECK(study_ui::selector(box,press(99,10),0,0).direction==0);
    CHECK(study_ui::selector(box,press(99,10),2,2).direction==0);
    CHECK(!study_ui::selector(box,press(99,10),0,2,false).next_enabled);
    for(study_ui::Rect bad:std::array<study_ui::Rect,5>{{{0,0,40,20},{0,0,20,20},{0,0,100,0},{0,0,100,-1},{0,0,std::numeric_limits<double>::infinity(),20}}}){
        const auto s=study_ui::selector(bad,hit,0,2);CHECK(!s.previous_enabled&&!s.next_enabled&&s.direction==0);
    }
    // Independent tables: columns are confirm(1), left(2), right(4) edge masks.
    constexpr Action table[5][8]={
        {Action::none,Action::start,Action::none,Action::start,Action::none,Action::start,Action::none,Action::start},
        {Action::none,Action::toggle_decorations,Action::none,Action::toggle_decorations,Action::none,Action::toggle_decorations,Action::none,Action::toggle_decorations},
        {Action::none,Action::none,Action::none,Action::none,Action::next_badge,Action::next_badge,Action::none,Action::none},
        {Action::none,Action::none,Action::previous_badge,Action::previous_badge,Action::none,Action::none,Action::none,Action::none},
        {Action::none,Action::account,Action::none,Action::account,Action::none,Action::account,Action::none,Action::account}
    };
    constexpr int navigation[4][4]={{0,0,1,0},{1,0,2,1},{2,1,3,2},{3,2,3,3}};
    std::size_t cases=0;
    for(int decoration=0;decoration<2;++decoration)for(int badge=0;badge<2;++badge){
        study_menu::Preferences prefs;
        if (!decoration) CHECK(prefs.apply(Action::toggle_decorations));
        if (badge) CHECK(prefs.apply(Action::next_badge));
        for(int focus=0;focus<4;++focus)for(int nav=0;nav<4;++nav)for(int mask=0;mask<8;++mask)for(bool absent:{false,true}){
            study_ui::Input input;input.cancelled=absent;
            const study_menu::Keys keys{bool(nav&1),bool(nav&2),bool(mask&2),bool(mask&4),bool(mask&1),false};
            const auto result=study_menu::evaluate(prefs,Focus(focus),input,keys);
            const int destination=navigation[focus][nav];
            CHECK(result.focus==Focus(destination)&&result.action==table[destination==3?4:destination==2?2+badge:destination][mask]);
            CHECK(prefs.decorations()==bool(decoration)&&prefs.badge()==std::size_t(badge));++cases;
            auto cancelled=keys;cancelled.cancelled=true;
            const auto none=study_menu::evaluate(prefs,Focus(focus),press(20,70),cancelled);
            CHECK(none.focus==Focus(focus)&&none.action==Action::none);
        }
        const study_menu::Keys simultaneous{false,true,false,true,true,false};
        const auto start=study_menu::evaluate(prefs,Focus::decorations,press(20,70),simultaneous);
        CHECK(start.focus==Focus::start&&start.action==Action::start);
        const auto toggle=study_menu::evaluate(prefs,Focus::start,press(140,90),simultaneous);
        CHECK(toggle.focus==Focus::decorations&&toggle.action==Action::toggle_decorations);
        const auto center=study_menu::evaluate(prefs,Focus::start,press(200,130),simultaneous);
        CHECK(center.focus==Focus::badge&&center.action==Action::none);
        const auto left=study_menu::evaluate(prefs,Focus::start,press(110,130),simultaneous);
        const auto right=study_menu::evaluate(prefs,Focus::start,press(289,130),simultaneous);
        CHECK(left.focus==Focus::badge&&left.action==(badge?Action::previous_badge:Action::none));
        CHECK(right.focus==Focus::badge&&right.action==(badge?Action::none:Action::next_badge));
    }
    study_menu::Preferences prefs;
    CHECK(!prefs.apply(Action::previous_badge)&&prefs.badge()==0);
    CHECK(prefs.apply(Action::next_badge)&&!prefs.apply(Action::next_badge)&&prefs.badge()==1);
    CHECK(!prefs.apply(Action::none)&&!prefs.apply(Action::start)&&!prefs.apply(Action(99)));
    CHECK(prefs.apply(Action::toggle_decorations)&&!prefs.decorations());
    CHECK(prefs.apply(Action::toggle_decorations)&&prefs.decorations()); // toggles are not idempotent
    CHECK(study_menu::evaluate(prefs,Focus(99),{},{}).focus==Focus::start);
    // Input view is pure: a repeated evaluation doesn't apply the returned intent.
    const auto request=press(140,90);const auto first=study_menu::evaluate(prefs,Focus::start,request,{});
    CHECK(study_menu::evaluate(prefs,Focus::start,request,{}).action==first.action&&prefs.decorations());
    CHECK(prefs.apply(first.action)&&!prefs.decorations());
    // Presentation state outlives a round and never changes rule state or restart baseline.
    const auto initial=study_round::Round::create_seeded(study_grid::Grid{},1);CHECK(initial);
    study_app::Application app(*initial);const auto initial_hash=study_hash::state_hash(*initial);
    CHECK(app.advance(0,{},true,false,true));CHECK(study_hash::state_hash(app.game()->round())==initial_hash);
    for(int i=0;i<100;++i){prefs.apply(Action::toggle_decorations);prefs.apply(Action::previous_badge);prefs.apply(Action::next_badge);CHECK(study_hash::state_hash(app.game()->round())==initial_hash);}
    CHECK(app.advance(0,{},false,true,true)&&app.screen()==study_app::Screen::menu);
    CHECK(!prefs.decorations()&&prefs.badge()==1);
    CHECK(app.advance(0,{},true,false,true)&&study_hash::state_hash(app.game()->round())==initial_hash);
    std::printf("Widget state: %zu keyboard/focus/channel table cases; endpoint consumption, one intent, checked snapshot, persistence and rule separation passed\n",cases);
}
