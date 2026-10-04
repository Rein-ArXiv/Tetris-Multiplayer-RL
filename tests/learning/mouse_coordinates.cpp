#include <algorithm>
#include <cstdint>
#include <limits>
#include <cstdio>
#include <cstdlib>
// Model Windows header macros even on Linux; the shared helper must survive them.
#define min(a, b) unexpected_windows_min_macro
#define max(a, b) unexpected_windows_max_macro
#include "platform/mouse_coordinates.h"
#undef min
#undef max
#define CHECK(...) do{if(!(__VA_ARGS__)){std::fprintf(stderr,"mouse line %d\n",__LINE__);std::exit(1);}}while(false)
int main(){
    using platform_detail::logical_mouse_axis;
    CHECK(logical_mouse_axis(351,352,1215,720)==-1);
    CHECK(logical_mouse_axis(352,352,1215,720)==0);
    CHECK(logical_mouse_axis(1566,352,1215,720)==719);
    CHECK(logical_mouse_axis(1567,352,1215,720)==720);
    CHECK(logical_mouse_axis(-2,0,2,1)==-1);
    CHECK(logical_mouse_axis(-3,0,2,1)==-2);
    CHECK(logical_mouse_axis(0,0,0,720)==-1);
    const int lo=std::numeric_limits<int>::min(),hi=std::numeric_limits<int>::max();
    CHECK(logical_mouse_axis(lo,hi,1,hi)==lo);
    CHECK(logical_mouse_axis(hi,lo,1,hi)==hi);
    CHECK(logical_mouse_axis(hi,lo,hi,hi)==hi);
    std::puts("Mouse mapping: negative edge, right edge, invalid size and int limits passed");
}
