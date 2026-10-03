#include "renderer/letterbox.h"
#include <cstdio>
#include <cstdlib>
#include <limits>
using namespace study_letterbox;
#define CHECK(...) do { if (!(__VA_ARGS__)) { std::fprintf(stderr,"letterbox line %d\n",__LINE__); std::exit(1); } } while(false)
static bool near(double a,double b) { return std::abs(a-b)<1e-8; }
int main() {
    const Size logical{320,240};
    auto wide=make_layout({1000,400},{2000,800},logical); CHECK(wide);
    CHECK(wide->viewport.x==467 && wide->viewport.y==0 && wide->viewport.width==1066 && wide->viewport.height==800);
    auto p=window_to_logical(*wide,{500,200}); CHECK(p && near(p->x,160) && near(p->y,120));
    CHECK(!window_to_logical(*wide,{233,200})); // -fraction must not become logical 0
    CHECK(!window_to_logical(*wide,{766.5,200})); // right edge excluded
    p=window_to_logical(*wide,{233.5,0}); CHECK(p && p->x==0 && p->y==0);
    auto odd=make_layout({801,603},{801,603},logical); CHECK(odd);
    CHECK(odd->viewport.y==1 && odd->viewport.height==600 && gl_y(*odd)==2);
    CHECK(!window_to_logical(*odd,{400,0}));
    CHECK(!window_to_logical(*odd,{400,601}));
    CHECK(window_to_logical(*odd,{400,600.999}));
    // Different x/y ratios: infer each scale, do not hard-code DPI=2.
    auto unequal=make_layout({500,300},{1250,900},logical); CHECK(unequal);
    for (Point q : {Point{0,0},Point{40,60},Point{160,120},Point{319,239}}) {
        auto w=logical_to_window(*unequal,q); CHECK(w);
        auto r=window_to_logical(*unequal,*w); CHECK(r && near(r->x,q.x) && near(r->y,q.y));
    }
    CHECK(logical_to_window(*odd,{320,240}));
    CHECK(!logical_to_window(*odd,{320.1,240}));
    for (Size bad : {Size{0,1},Size{1,0},Size{-1,1}}) {
        CHECK(!make_layout(bad,{640,480},logical));
        CHECK(!make_layout({640,480},bad,logical));
        CHECK(!make_layout({640,480},{640,480},bad));
    }
    CHECK(!make_layout({1,1},{1,1},{320,1})); // fitted height rounds to 0
    const int max=std::numeric_limits<int>::max();
    auto huge=make_layout({max,max},{max,max},{max,max}); CHECK(huge && huge->viewport.width==max);
    auto thin=make_layout({1,max},{1,max},{1,max}); CHECK(thin && thin->viewport.height==max);
    CHECK(!window_to_logical(*odd,{std::numeric_limits<double>::quiet_NaN(),0}));
    CHECK(!window_to_logical(*odd,{0,std::numeric_limits<double>::infinity()}));
    CHECK(!logical_to_window(*odd,{std::numeric_limits<double>::infinity(),0}));
    // Sweep odd/even sizes: containment, one-pixel centering remainder, and
    // ratio floor inequalities. A dropped y flip or rounded side fails these.
    for(int w=1;w<120;++w) for(int h=1;h<120;++h) {
        auto l=make_layout({w,h},{w,h},logical);
        if(!l) { CHECK(w==1 && h>=1); continue; }
        const auto v=l->viewport;
        CHECK(v.width<=w && v.height<=h && v.width>0 && v.height>0);
        CHECK(w-2*v.x-v.width>=0 && w-2*v.x-v.width<=1);
        CHECK(h-2*v.y-v.height>=0 && h-2*v.y-v.height<=1);
        CHECK(gl_y(*l)==h-v.y-v.height);
        if(w*240<=h*320) CHECK(v.width==w && v.height*320<=w*240 && (v.height+1)*320>w*240);
        else CHECK(v.height==h && v.width*240<=h*320 && (v.width+1)*240>h*320);
    }
    std::puts("Letterbox: fit, odd margins, DPI axes, half-open input, round trips, limits passed");
}
