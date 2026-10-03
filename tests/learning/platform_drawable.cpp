#include "platform/platform.h"
#include <cstdlib>
#include <cstdio>
static void require(bool v) {if(!v)std::exit(1);}
int main(int argc,char**) {
    auto size=platform_drawable_size();require(size.width==0&&size.height==0);
    platform_present();
    require(platform_init(320,240,"drawable contract"));
    size=platform_drawable_size();
    require(argc>1 ? size.width==0&&size.height==0 : size.width==1280&&size.height==960);
    platform_present();
    platform_shutdown();size=platform_drawable_size();require(size.width==0&&size.height==0);
    platform_present();platform_shutdown();
    std::puts("Platform drawable/present: inactive, real-pixel units, unavailable-surface gating passed (double)");
}
