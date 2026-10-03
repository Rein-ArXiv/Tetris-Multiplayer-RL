#include "renderer/quad.h"
#include <cstdio>
int main(){
    using namespace study_raster;
    const std::array<Point,3> first{{{4,4},{12,4},{12,12}}};
    const std::array<Point,3> second{{{4,4},{12,12},{4,12}}};
    int closed_duplicates=0,strict_holes=0,owned_holes=0,owned_duplicates=0;
    for(int y=15;y>=0;--y){
        for(int x=0;x<16;++x){
            const auto p=pixel_center(x,y);
            const auto a=sample_triangle(first,p),b=sample_triangle(second,p);
            const auto own_a=study_quad::covers_top_left(first,p),own_b=study_quad::covers_top_left(second,p);
            if(!a||!b||!own_a||!own_b)return 1; // optional validity, not its bool value
            const int closed=(a->region!=Region::outside)+(b->region!=Region::outside);
            const int strict=(a->region==Region::inside)+(b->region==Region::inside);
            const int owned=int(*own_a)+int(*own_b);
            const bool expected=x>=4&&x<12&&y>=4&&y<12;
            if(expected){
                closed_duplicates+=closed==2;strict_holes+=strict==0;
                owned_holes+=owned==0;owned_duplicates+=owned==2;
            }
            std::putchar(owned==0?'.':(*own_a?'A':'B'));
        }
        std::putchar('\n');
    }
    std::printf("closed duplicates=%d; strict holes=%d; owned holes=%d duplicates=%d\n",
                closed_duplicates,strict_holes,owned_holes,owned_duplicates);
    return owned_holes||owned_duplicates?1:0;
}
