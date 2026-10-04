#include "renderer/blend.h"
#include <cstdio>
int main() {
    using namespace study_blend;
    const Rgba red{1,0,0,0.5}, blue{0,0,1,0.5};
    for (double background_alpha : {0.0,1.0}) {
        const Rgba background{0,0,0,background_alpha};
        const auto red_first = over_straight(red,background);
        const auto blue_first = over_straight(blue,background);
        if (!red_first || !blue_first) return 1;
        const auto ab = over_straight(blue,*red_first);
        const auto ba = over_straight(red,*blue_first);
        if (!ab || !ba) return 1;
        std::printf("background alpha %.0f | A then B: %.2f %.2f %.2f %.2f | B then A: %.2f %.2f %.2f %.2f\n",
            background_alpha,ab->r,ab->g,ab->b,ab->a,ba->r,ba->g,ba->b,ba->a);
    }
}
