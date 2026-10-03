#include "platform/sdl.cpp"
// Test-only fixture: real SDL queue/pump, no GL window or platform_init claim.
#include "simulation/pending_controls.h"
#ifndef SDL_MAIN_HANDLED
#define SDL_MAIN_HANDLED
#endif
#include <SDL.h>
#include <cstdio>
static bool push(Uint32 type,Uint8 repeat=0) {
    SDL_Event e{};e.type=type;e.key.keysym.sym=SDLK_UP;e.key.repeat=repeat;return SDL_PushEvent(&e)==1;
}
int main() {
    active=std::make_unique<Resources>();
    if(!active->runtime.ready()){platform_shutdown();return 1;}
    active->frequency=SDL_GetPerformanceFrequency();
    if(!active->frequency){platform_shutdown();return 1;}
    active->previous=active->frame_start=SDL_GetPerformanceCounter();
    int result=1;
    do {
        platform_begin_frame();
        if(!push(SDL_KEYDOWN))break;
        platform_begin_frame();if(!platform_key_pressed(Key::Up)||!platform_key_down(Key::Up))break;
        study_input::PendingControls pending;pending.capture(false,false,platform_key_pressed(Key::Up));
        platform_begin_frame();if(platform_key_pressed(Key::Up)||!platform_key_down(Key::Up))break;
        pending.capture(false,false,platform_key_pressed(Key::Up));
        if(!push(SDL_KEYDOWN,1))break;
        platform_begin_frame();if(platform_key_pressed(Key::Up))break;
        if(!pending.consume().clockwise||pending.consume().clockwise)break;
        if(!push(SDL_KEYUP))break;
        platform_begin_frame();if(!platform_key_released(Key::Up)||platform_key_down(Key::Up))break;
        if(!push(SDL_KEYDOWN))break;
        platform_begin_frame();if(!platform_key_pressed(Key::Up))break;
        SDL_Event lost{};lost.type=SDL_WINDOWEVENT;lost.window.event=SDL_WINDOWEVENT_FOCUS_LOST;
        if(SDL_PushEvent(&lost)!=1)break;
        platform_begin_frame();if(platform_key_down(Key::Up))break;
        result=0;
    } while(false);
    platform_shutdown();
    if(!result)std::puts("Actual SDL queue/pump Up press/hold/repeat/release/focus-loss and pending-once passed (test fixture; no platform_init)");
    return result;
}
