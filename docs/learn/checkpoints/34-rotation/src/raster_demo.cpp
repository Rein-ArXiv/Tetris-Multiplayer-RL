#include "renderer/raster.h"
#include <cstdio>
int main() {
    using namespace study_raster;
    const std::array<Point,3> triangle{{{4,4},{12,4},{8,12}}};
    // Print the top row first, although window y increases upward.
    for(int y=15;y>=0;--y){
        for(int x=0;x<16;++x){
            const auto sample=sample_triangle(triangle,pixel_center(x,y));
            if(!sample)return 1;
            const char ch=sample->region==Region::inside?'#':
                          sample->region==Region::boundary?'B':'.';
            std::putchar(ch);
        }
        std::putchar('\n');
    }
    const auto sample=sample_triangle(triangle,{8,6});
    if(!sample)return 1;
    const auto rgb=mix_rgb(sample->weights);
    std::printf("Point (8,6): weights/RGB=(%.3f,%.3f,%.3f)\n",rgb[0],rgb[1],rgb[2]);
    std::puts("# strict interior, B exact boundary, . outside; shared-edge ownership is not modeled");
}
