#include "client/immediate_ui.h"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <limits>
#define CHECK(e) do{if(!(e)){std::fprintf(stderr,"CHECK %d: %s\n",__LINE__,#e);std::exit(1);}}while(false)
int main() {
    using study_pointer::Point;
    study_pointer::Edges edges;
    CHECK(!edges.snapshot().position&&!edges.snapshot().press&&!edges.snapshot().down);
    edges.begin_frame();edges.set(true,{10,20});edges.set(false,{10,20});edges.move({90,80});
    auto frame=edges.snapshot();CHECK(frame.press&&frame.press->x==10&&frame.position->x==90&&!frame.down);
    edges.set(true,{30,40});CHECK(edges.snapshot().press->x==10&&edges.snapshot().down); // first edge retained
    edges.begin_frame();CHECK(!edges.snapshot().press&&edges.snapshot().down);
    edges.set(true,{50,60});CHECK(!edges.snapshot().press); // duplicate down is not another edge
    edges.set(false,{50,60});edges.set(true,{70,80});CHECK(edges.snapshot().press->x==70);
    edges.cancel();edges.set(true,{1,2});frame=edges.snapshot();CHECK(frame.cancelled&&!frame.press&&!frame.position&&!frame.down);
    edges.begin_frame();frame=edges.snapshot();CHECK(!frame.cancelled&&frame.down&&!frame.press); // later native state, no resurrected click
    edges.reset();CHECK(!edges.snapshot().position&&!edges.snapshot().down);
    using study_ui::Rect;
    CHECK(study_ui::contains({0,0,10,10},{0,0}));
    CHECK(!study_ui::contains({0,0,10,10},{10,5})&&study_ui::contains({10,0,10,10},{10,5}));
    CHECK(!study_ui::contains({0,0,10,10},{5,10}));
    const double inf=std::numeric_limits<double>::infinity(),nan=std::numeric_limits<double>::quiet_NaN(),max=std::numeric_limits<double>::max();
    for(Rect bad:std::array<Rect,7>{{{0,0,0,1},{0,0,-1,1},{0,0,1,-1},{nan,0,1,1},{0,0,inf,1},{max,0,max,1},{max,0,1,1}}})CHECK(!study_ui::contains(bad,{0,0}));
    CHECK(!study_ui::contains({0,0,10,10},{nan,1}));
    std::size_t cases=0;
    // Use integer inequalities as an independent oracle for exact half-unit points.
    for(int x=-5;x<=5;++x)for(int y=-5;y<=5;++y)for(int w=-1;w<=5;++w)for(int h=-1;h<=5;++h)
    for(int px=-12;px<=20;++px)for(int py=-12;py<=20;++py) {
        const bool expected=w>0&&h>0&&px>=2*x&&px<2*(x+w)&&py>=2*y&&py<2*(y+h);
        CHECK(study_ui::contains({double(x),double(y),double(w),double(h)},{px*.5,py*.5})==expected);++cases;
    }
    const auto layout=study_letterbox::make_layout({800,600},{1600,1200},{320,240});CHECK(layout);
    frame={Point{790,590},Point{100,200},false,false};
    auto input=study_ui::map_pointer(layout,frame);
    CHECK(input.position&&input.position->x==316&&input.press&&input.press->x==40&&input.press->y==80);
    const auto clicked=study_ui::button({30,70,20,20},input);
    CHECK(!clicked.hovered&&!clicked.held&&clicked.activated); // event origin, not final cursor
    CHECK(!study_ui::button({30,70,20,20},input,false).activated);
    input.cancelled=true;CHECK(!study_ui::button({30,70,20,20},input).activated);
    const auto wide=study_letterbox::make_layout({800,400},{1600,800},{320,240});CHECK(wide);
    frame={Point{1,1},Point{1,1},true,false};input=study_ui::map_pointer(wide,frame);
    CHECK(!input.position&&!input.press&&!study_ui::button({0,0,320,240},input).activated);
    input=study_ui::map_pointer(std::nullopt,frame);CHECK(!input.position&&!input.press&&!input.down);
    frame.cancelled=true;input=study_ui::map_pointer(layout,frame);CHECK(input.cancelled&&!input.press&&!input.position&&!input.down);
    // Identical snapshot and bounds yield identical intent; dispatch is the caller's job.
    input={study_letterbox::Point{5,5},study_letterbox::Point{5,5},true,false};
    const auto a=study_ui::button({0,0,10,10},input),b=study_ui::button({0,0,10,10},input);
    CHECK(a.hovered&&a.held&&a.activated&&b.activated);
    std::printf("Immediate UI: %zu exact geometry cases; first press origin, quick click, cancellation, HiDPI/letterbox mapping and intent passed\n",cases);
}
