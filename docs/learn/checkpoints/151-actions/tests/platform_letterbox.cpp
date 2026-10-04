#include "platform/platform.h"
#include "renderer/letterbox.h"
#ifndef SDL_MAIN_HANDLED
#define SDL_MAIN_HANDLED
#endif
#include <SDL.h>
#include <cstdio>
#include <cstdlib>
#define CHECK(...) do{if(!(__VA_ARGS__)){std::fprintf(stderr,"platform letterbox line %d\n",__LINE__);std::exit(1);}}while(false)
int main(){
    CHECK(platform_window_size().width==0 && !platform_window_mouse().available);
    CHECK(platform_init(1000,400,"coordinate routing fixture"));
    platform_begin_frame();
    const auto w=platform_window_size();const auto d=platform_drawable_size();
    CHECK(w.width==1000&&w.height==400&&d.width==2000&&d.height==800);
    const auto l=study_letterbox::make_layout({w.width,w.height},{d.width,d.height},{320,240});CHECK(l);
    auto m=platform_window_mouse();CHECK(m.available&&m.x==500&&m.y==200);
    auto p=study_letterbox::window_to_logical(*l,{double(m.x),double(m.y)});CHECK(p&&p->x==160&&p->y==120);
    SDL_setenv("LEARN_GL","bar",1);m=platform_window_mouse();
    CHECK(!study_letterbox::window_to_logical(*l,{double(m.x),double(m.y)}));
    SDL_setenv("LEARN_GL","no-focus",1);CHECK(!platform_window_mouse().available);
    SDL_setenv("LEARN_GL","minimized",1);
    CHECK(platform_window_size().width==0&&platform_drawable_size().width==0&&!platform_window_mouse().available);
    SDL_setenv("LEARN_GL","zero-drawable",1);
    CHECK(platform_drawable_size().width==0);
    SDL_setenv("LEARN_GL","okay",1);
    SDL_Event e{};e.type=SDL_QUIT;CHECK(SDL_PushEvent(&e)==1);platform_begin_frame();
    CHECK(platform_window_size().width==0&&!platform_window_mouse().available);
    platform_shutdown();CHECK(platform_window_size().width==0&&!platform_window_mouse().available);
    std::puts("Window/drawable/mouse routing: distinct sizes, bars, focus, minimized, zero, closing passed");
}
