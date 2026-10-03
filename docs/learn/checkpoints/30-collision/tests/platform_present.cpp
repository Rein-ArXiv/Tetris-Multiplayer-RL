// Linux fixture checks the real platform implementation's routing policy only.
#include "platform/platform.h"
#ifndef SDL_MAIN_HANDLED
#define SDL_MAIN_HANDLED
#endif
#include <SDL.h>
#include <dlfcn.h>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#define CHECK(x) do {if(!(x)){std::fprintf(stderr,"platform present line %d\n",__LINE__);std::exit(1);}}while(0)
int main(){
    const auto swaps=reinterpret_cast<int(*)()>(dlsym(RTLD_DEFAULT,"study_swap_calls"));
    const auto requests=reinterpret_cast<int(*)()>(dlsym(RTLD_DEFAULT,"study_interval_requests"));
    CHECK(swaps&&requests);
    CHECK(!platform_present()&&!platform_set_swap_interval(1).attempted);
    CHECK(platform_init(128,128,"present policy fixture"));
    auto r=platform_set_swap_interval(1);CHECK(r.attempted&&r.accepted&&r.reported_interval==1&&requests()==1);
    CHECK(platform_present()&&swaps()==1);
    CHECK(!platform_set_swap_interval(-1).attempted&&requests()==1);
    CHECK(!platform_set_swap_interval(2).attempted&&requests()==1);
    for(const char* mode : {"current-fail","current-window-fail"}){
        SDL_setenv("LEARN_GL",mode,1);CHECK(!platform_present()&&!platform_set_swap_interval(1).attempted);
        CHECK(swaps()==1&&requests()==1);
    }
    SDL_setenv("LEARN_GL","zero-drawable",1);CHECK(!platform_present()&&swaps()==1);
    SDL_setenv("LEARN_GL","swap-fail",1);r=platform_set_swap_interval(0);
    CHECK(r.attempted&&!r.accepted&&r.reported_interval==1&&requests()==2);
    SDL_setenv("LEARN_GL","report-unknown",1);r=platform_set_swap_interval(1);
    CHECK(r.attempted&&r.accepted&&r.reported_interval==0&&requests()==3);
    SDL_setenv("LEARN_GL","okay",1);
    SDL_Event e{};e.type=SDL_QUIT;CHECK(SDL_PushEvent(&e)==1);platform_begin_frame();
    CHECK(!platform_present()&&!platform_set_swap_interval(1).attempted);
    platform_shutdown();platform_shutdown();CHECK(!platform_present()&&!platform_set_swap_interval(1).attempted);
    CHECK(swaps()==1&&requests()==3);
    std::puts("Platform dispatch: inactive, current mismatch, zero drawable, failed/unknown interval and closing passed");
}
