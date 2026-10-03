#include "renderer/letterbox.h"
#include <cstdio>
int main() {
    using namespace study_letterbox;
    const auto l=make_layout({1000,400},{2000,800},{320,240});
    if(!l)return 1;
    const auto v=l->viewport;
    std::printf("viewport top-left: (%d,%d) %dx%d; GL y=%d\n",v.x,v.y,v.width,v.height,gl_y(*l));
    for (Point p : {Point{500,200},Point{233,200},Point{233.5,0},Point{766.5,200}}) {
        const auto q=window_to_logical(*l,p);
        if(q)std::printf("window (%.1f,%.1f) -> logical (%.2f,%.2f)\n",p.x,p.y,q->x,q->y);
        else std::printf("window (%.1f,%.1f) -> outside\n",p.x,p.y);
    }
}
