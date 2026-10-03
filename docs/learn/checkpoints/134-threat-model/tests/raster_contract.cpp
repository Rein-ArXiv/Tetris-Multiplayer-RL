#include "renderer/raster.h"
#include <algorithm>
#include <climits>
#include <cstdio>
#include <limits>
using namespace study_raster;
#define CHECK(t) do{if(!(t)){std::fprintf(stderr,"line %d: %s\n",__LINE__,#t);return 1;}}while(false)
static bool near(double a,double b){return std::abs(a-b)<1e-10;}
int main(){
    const std::array<Point,3> tri{{{0,0},{4,0},{0,4}}};
    CHECK(edge(tri[0],tri[1],tri[2])==16);
    auto s=sample_triangle(tri,{1,1});
    CHECK(s && s->region==Region::inside);
    CHECK(near(s->weights.a,.5)&&near(s->weights.b,.25)&&near(s->weights.c,.25));
    auto rgb=mix_rgb(s->weights);CHECK(near(rgb[0],.5)&&near(rgb[1],.25)&&near(rgb[2],.25));
    s=sample_triangle(tri,{0,0});CHECK(s&&s->region==Region::boundary&&s->weights.a==1);
    s=sample_triangle(tri,{2,2});CHECK(s&&s->region==Region::boundary&&s->weights.a==0);
    s=sample_triangle(tri,{3,3});CHECK(s&&s->region==Region::outside&&s->weights.a<0);
    // A point on an edge's extension remains outside, not boundary.
    s=sample_triangle(tri,{5,0});CHECK(s&&s->region==Region::outside);
    auto reverse=tri;std::swap(reverse[1],reverse[2]);
    s=sample_triangle(reverse,{1,2});CHECK(s&&s->region==Region::inside);
    CHECK(near(s->weights.a,.25)&&near(s->weights.b,.5)&&near(s->weights.c,.25));
    auto shifted=tri;for(auto& v:shifted){v.x+=100;v.y-=100;}
    s=sample_triangle(shifted,{101,-99});CHECK(s&&s->region==Region::inside&&near(s->weights.a,.5));
    CHECK(!sample_triangle({{{0,0},{1,1},{2,2}}},{1,1}));
    CHECK(!sample_triangle({{{0,0},{0,0},{2,2}}},{1,1}));
    CHECK(!sample_triangle(tri,{1000001,0}));
    CHECK(!sample_triangle(tri,{std::numeric_limits<double>::infinity(),0}));
    auto invalid=tri;invalid[0].x=std::numeric_limits<double>::quiet_NaN();
    CHECK(!sample_triangle(invalid,{1,1}));
    const auto p=pixel_center(1,2);CHECK(p.x==1.5&&p.y==2.5);
    CHECK(pixel_center(INT_MAX,INT_MIN).x==double(INT_MAX)+.5);
    CHECK(pixel_center(INT_MAX,INT_MIN).y==double(INT_MIN)+.5);
    int inside=0,boundary=0,outside=0;
    const std::array<Point,3> display{{{4,4},{12,4},{8,12}}};
    for(int y=0;y<16;++y)for(int x=0;x<16;++x){
        s=sample_triangle(display,pixel_center(x,y));CHECK(s);
        if(s->region==Region::inside)++inside;
        else if(s->region==Region::boundary)++boundary;
        else ++outside;
        CHECK(near(s->weights.a+s->weights.b+s->weights.c,1));
    }
    CHECK(inside==32&&boundary==0&&outside==224);
    // Deliberately hit a shared diagonal: this helper reports boundary, not owner.
    const auto lower=sample_triangle(tri,pixel_center(1,2));
    CHECK(lower&&lower->region==Region::boundary);
    const auto upper=sample_triangle({{{4,0},{4,4},{0,4}}},pixel_center(1,2));
    CHECK(upper&&upper->region==Region::boundary);
    std::puts("Raster contracts: signed edges, weights, both windings, invalid input, pixel centers and diagnostic boundaries passed");
}
