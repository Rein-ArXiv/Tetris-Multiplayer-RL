#include "renderer/coordinates.h"
#include "renderer/projection_cases.h"
#include "renderer/mesh.h"
#include <climits>
#include <cstdio>
#include <limits>
using namespace study_coordinates;
#define CHECK(test) do { if (!(test)) { std::fprintf(stderr,"line %d: %s\n",__LINE__,#test); return 1; } } while(false)
static bool near(double a,double b){return std::abs(a-b)<1e-10;}
int main() {
    CHECK(inside_clip_volume({1,-1,1,1}));
    CHECK(!inside_clip_volume({1.01,0,0,1}));
    CHECK(!inside_clip_volume({0,0,0,0}));
    CHECK(!inside_clip_volume({0,0,0,-1}));
    CHECK(!inside_clip_volume({0,0,1.01,1}));
    const double inf=std::numeric_limits<double>::infinity();
    const double nan=std::numeric_limits<double>::quiet_NaN();
    const double largest=std::numeric_limits<double>::max();
    CHECK(!inside_clip_volume({nan,0,0,1}));
    CHECK(!inside_clip_volume({0,0,0,inf}));
    CHECK(!perspective_divide({1,2,3,0}));
    CHECK(!perspective_divide({1,2,3,-0.0}));
    CHECK(!perspective_divide({inf,0,0,1}));
    CHECK(!perspective_divide({0,0,nan,1}));
    CHECK(!perspective_divide({largest,0,0,0.5}));
    auto n=perspective_divide({1,2,3,-2});
    CHECK(n && near(n->x,-0.5) && near(n->y,-1) && near(n->z,-1.5));
    n=perspective_divide({largest,largest,0,largest});
    CHECK(n && near(n->x,1) && near(n->y,1));
    const Viewport vp{10,20,200,100};
    auto w=to_window({1,1,1},vp);
    CHECK(w && near(w->x,210) && near(w->y,120) && near(w->depth,1));
    w=to_window({-1,-1,-1},vp);
    CHECK(w && near(w->x,10) && near(w->y,20) && near(w->depth,0));
    w=to_window({0.5,-0.5,0},{-20,-30,200,100});
    CHECK(w && near(w->x,130) && near(w->y,-5) && near(w->depth,0.5));
    w=to_window({1,1,0},{INT_MAX,INT_MAX,INT_MAX,INT_MAX});
    CHECK(w && near(w->x,2.0*INT_MAX) && near(w->y,2.0*INT_MAX));
    CHECK(!to_window({0,0,0},{0,0,0,100}));
    CHECK(!to_window({0,0,0},{0,0,100,-1}));
    CHECK(!to_window({0,nan,0},vp));
    CHECK(!to_window({largest,0,0},vp));
    w=to_window({2,0,2},vp);CHECK(w && near(w->x,310) && near(w->depth,1.5));
    auto c=ui_to_clip(0,0,720,640);
    CHECK(c && c->x==-1 && c->y==1 && c->z==0 && c->w==1);
    c=ui_to_clip(360,320,720,640);CHECK(c && c->x==0 && c->y==0);
    c=ui_to_clip(720,640,720,640);CHECK(c && c->x==1 && c->y==-1);
    c=ui_to_clip(1080,-320,720,640);CHECK(c && c->x==2 && c->y==2);
    c=ui_to_clip(largest,largest,largest,largest);CHECK(c && c->x==1 && c->y==-1);
    CHECK(!ui_to_clip(0,0,0,640));CHECK(!ui_to_clip(0,0,720,-1));
    CHECK(!ui_to_clip(0,nan,720,640));CHECK(!ui_to_clip(0,0,inf,640));
    CHECK(!ui_to_clip(largest,0,0.5,640));
    // Independent expected coordinates for all three vertices and four cases.
    const double expected[4][3][2]={
        {{60,45},{160,45},{110,95}},
        {{85,57.5},{135,57.5},{110,82.5}},
        {{60,45},{160,45},{110,95}},
        {{-40,-5},{260,-5},{110,145}}
    };
    int ci=0;
    for(const auto& experiment:study_projection::cases){
        int vi=0;
        for(const auto& vertex:study_mesh::make_triangle()){
            c=study_projection::clip_for(experiment,vertex.x,vertex.y);
            n=perspective_divide(*c);CHECK(n);w=to_window(*n,vp);CHECK(w);
            CHECK(near(w->x,expected[ci][vi][0]) && near(w->y,expected[ci][vi][1]));
            CHECK(inside_clip_volume(*c)==(ci!=3));++vi;
        }
        ++ci;
    }
    CHECK(study_projection::find_case("missing")==nullptr);
    std::puts("Coordinate contracts: clip predicate, divide, viewport, UI, four cases and invalid inputs passed");
}
