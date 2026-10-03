#include "renderer/quad.h"
#include <cstdio>
#include <limits>
using namespace study_raster;
#define CHECK(t) do{if(!(t)){std::fprintf(stderr,"quad line %d: %s\n",__LINE__,#t);return 1;}}while(false)
int main(){
    const auto mesh=study_quad::make_quad();CHECK(mesh.size()==6 && sizeof(mesh)==48);
    CHECK(mesh[0].x==mesh[3].x&&mesh[0].y==mesh[3].y);
    CHECK(mesh[2].x==mesh[4].x&&mesh[2].y==mesh[4].y);
    const auto reversed=study_quad::make_quad(true);CHECK(reversed[1].x==mesh[2].x&&reversed[1].y==mesh[2].y);
    std::array<Point,3> a{{{4,4},{12,4},{12,12}}},b{{{4,4},{12,12},{4,12}}};
    int closed_duplicates=0,strict_holes=0;
    for(int y=0;y<16;++y)for(int x=0;x<16;++x){
        const auto p=pixel_center(x,y);
        const auto sa=sample_triangle(a,p),sb=sample_triangle(b,p);CHECK(sa&&sb);
        const auto ca=study_quad::covers_top_left(a,p),cb=study_quad::covers_top_left(b,p);CHECK(ca&&cb);
        const bool expected=x>=4&&x<12&&y>=4&&y<12;
        CHECK(int(*ca)+int(*cb)==int(expected));
        closed_duplicates+=(sa->region!=Region::outside)+(sb->region!=Region::outside)==2;
        strict_holes+=expected&&sa->region!=Region::inside&&sb->region!=Region::inside;
        auto reversed_a=a;std::swap(reversed_a[1],reversed_a[2]);
        const auto cr=study_quad::covers_top_left(reversed_a,p);CHECK(cr&&*cr==*ca);
    }
    CHECK(closed_duplicates==8&&strict_holes==8);
    auto inside=study_quad::covers_top_left(a,{8,8});CHECK(inside&&*inside);
    inside=study_quad::covers_top_left(b,{8,8});CHECK(inside&&!*inside);
    // Top/left outer edges included, bottom/right excluded for this CPU policy.
    inside=study_quad::covers_top_left(b,{4,8});CHECK(inside&&*inside);
    inside=study_quad::covers_top_left(b,{8,12});CHECK(inside&&*inside);
    inside=study_quad::covers_top_left(a,{8,4});CHECK(inside&&!*inside);
    inside=study_quad::covers_top_left(a,{12,8});CHECK(inside&&!*inside);
    CHECK(!study_quad::covers_top_left({{{0,0},{1,1},{2,2}}},{1,1}));
    CHECK(!study_quad::covers_top_left(a,{std::numeric_limits<double>::infinity(),0}));
    std::puts("Quad: six vertices, shared diagonal, both windings, top-left boundary ownership and invalid inputs passed");
}
