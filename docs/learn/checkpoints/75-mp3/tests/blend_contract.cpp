#include "renderer/blend.h"
#include <cstdio>
#include <cstdlib>
#include <limits>
using namespace study_blend;
#define CHECK(x) do {if(!(x)){std::fprintf(stderr,"blend line %d\n",__LINE__);std::exit(1);}}while(0)
static bool equal(Rgba a,Rgba b) {
    return a.r==b.r && a.g==b.g && a.b==b.b && a.a==b.a;
}
int main() {
    const Rgba red{1,0,0,0.5}, blue{0,0,1,0.5}, clear{0,0,0,0};
    const auto a=over_straight(red,clear),b=over_straight(blue,clear);
    CHECK(a && b);
    const auto ab=over_straight(blue,*a),ba=over_straight(red,*b);
    CHECK(ab && ba && equal(*ab,{0.25,0,0.5,0.75}) && equal(*ba,{0.5,0,0.25,0.75}));
    CHECK(!equal(*ab,*ba));
    const auto pa=premultiply(red),pb=premultiply(blue);
    CHECK(pa && pb);
    const auto same=over_premultiplied(*pb,*pa);
    CHECK(same && equal(*same,*ab));
    const auto zero=over_straight({1,1,1,0},*ab),one=over_straight({0,1,0,1},*ab);
    CHECK(zero && one && equal(*zero,*ab) && equal(*one,{0,1,0,1}));
    const auto opaque_a=over_straight(red,{0,0,0,1});CHECK(opaque_a);
    const auto opaque_ab=over_straight(blue,*opaque_a);CHECK(opaque_ab && opaque_ab->a==1);
    const double nan=std::numeric_limits<double>::quiet_NaN(),inf=std::numeric_limits<double>::infinity();
    for(Rgba invalid : {Rgba{nan,0,0,1},{0,inf,0,1},{0,0,-0.1,1},{0,0,0,1.1}}){
        CHECK(!premultiply(invalid) && !over_straight(invalid,clear) && !over_straight(red,invalid));
    }
    CHECK(!over_premultiplied(red,clear)); // straight red is not premultiplied at alpha .5
    CHECK(!over_straight(red,{0.5,0,0,0}));
    std::puts("Blend: order, alpha, equivalent representations, endpoints and invalid inputs passed");
}
