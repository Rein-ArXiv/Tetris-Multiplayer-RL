#include "platform/sdl.cpp"
#include "simulation/pending_controls.h"
#include "simulation/round.h"
#include <cstdio>
static bool push(Uint32 type,Uint8 repeat=0) {
    SDL_Event e{};e.type=type;e.key.keysym.sym=SDLK_SPACE;e.key.repeat=repeat;
    return SDL_PushEvent(&e)==1;
}
int main() {
    active=std::make_unique<Resources>();if(!active->runtime.ready())return 1;
    active->frequency=SDL_GetPerformanceFrequency();if(!active->frequency)return 1;
    active->previous=active->frame_start=SDL_GetPerformanceCounter();
    int result=1;study_input::PendingControls input;
    do {
        platform_begin_frame();if(!push(SDL_KEYDOWN))break;
        platform_begin_frame();if(!platform_key_pressed(Key::Space))break;
        input.capture(false,false,false,false,platform_key_pressed(Key::Space));
        // First frame runs zero ticks; next frame still sees held, not a new edge.
        platform_begin_frame();if(platform_key_pressed(Key::Space)||!platform_key_down(Key::Space))break;
        input.capture(false,false,false,false,platform_key_pressed(Key::Space));
        if(!input.consume().hard_drop||input.consume().hard_drop||input.consume().hard_drop)break;
        if(!push(SDL_KEYDOWN,1))break;
        platform_begin_frame();if(platform_key_pressed(Key::Space))break;
        if(!push(SDL_KEYUP))break;
        platform_begin_frame();if(!platform_key_released(Key::Space))break;
        if(!push(SDL_KEYDOWN))break;
        platform_begin_frame();if(!platform_key_pressed(Key::Space))break;
        // Current snapshot platform can miss a complete press/release in one pump.
        if(!push(SDL_KEYUP))break;
        platform_begin_frame();if(!push(SDL_KEYDOWN)||!push(SDL_KEYUP))break;
        platform_begin_frame();if(platform_key_pressed(Key::Space)||platform_key_down(Key::Space))break;
        if(!push(SDL_KEYDOWN))break;
        platform_begin_frame();
        SDL_Event lost{};lost.type=SDL_WINDOWEVENT;lost.window.event=SDL_WINDOWEVENT_FOCUS_LOST;
        if(SDL_PushEvent(&lost)!=1)break;
        platform_begin_frame();if(platform_key_down(Key::Space))break;
        result=0;
    }while(false);
    platform_shutdown();
    if(!result)std::puts("SDL Space edge/hold/repeat/release/repress/zero-tick latch/same-pump tap/focus-loss fixture passed");
    return result;
}
