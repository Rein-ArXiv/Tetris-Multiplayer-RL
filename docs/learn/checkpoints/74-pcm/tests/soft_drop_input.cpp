#include "platform/sdl.cpp"
#include "simulation/pending_controls.h"
#include "simulation/soft_drop.h"
#include <cstdio>
static bool push(Uint32 type,Uint8 repeat=0){SDL_Event e{};e.type=type;e.key.keysym.sym=SDLK_DOWN;e.key.repeat=repeat;return SDL_PushEvent(&e)==1;}
int main(){
    active=std::make_unique<Resources>();if(!active->runtime.ready())return 1;
    active->frequency=SDL_GetPerformanceFrequency();if(!active->frequency)return 1;
    active->previous=active->frame_start=SDL_GetPerformanceCounter();
    int result=1;study_input::PendingControls input;study_soft_drop::Counter counter;using R=study_soft_drop::Result;
    do{
        platform_begin_frame();if(!push(SDL_KEYDOWN))break;
        platform_begin_frame();
        if(!platform_key_down(Key::Down)||!platform_key_pressed(Key::Down))break;
        input.capture(false,false,false,platform_key_down(Key::Down));
        if(study_soft_drop::tick(input.consume().soft_drop,counter)!=R::due)break;
        if(!push(SDL_KEYDOWN,1))break;
        platform_begin_frame();
        if(platform_key_pressed(Key::Down)||!platform_key_down(Key::Down))break;
        input.capture(false,false,false,platform_key_down(Key::Down));
        bool ok=true;for(int n=0;n<3;++n)if(study_soft_drop::tick(input.consume().soft_drop,counter)!=R::waiting)ok=false;
        if(!ok||study_soft_drop::tick(input.consume().soft_drop,counter)!=R::due)break;
        if(!push(SDL_KEYUP))break;
        platform_begin_frame();
        input.capture(false,false,false,platform_key_down(Key::Down));
        if(!platform_key_released(Key::Down)||study_soft_drop::tick(input.consume().soft_drop,counter)!=R::idle)break;
        // A whole tap in one pump leaves the held sample false.
        if(!push(SDL_KEYDOWN)||!push(SDL_KEYUP))break;
        platform_begin_frame();
        if(platform_key_down(Key::Down))break;
        if(!push(SDL_KEYDOWN))break;
        platform_begin_frame();
        SDL_Event lost{};lost.type=SDL_WINDOWEVENT;lost.window.event=SDL_WINDOWEVENT_FOCUS_LOST;
        if(SDL_PushEvent(&lost)!=1)break;
        platform_begin_frame();
        input.capture(false,false,false,platform_key_down(Key::Down));
        if(platform_key_down(Key::Down)||input.consume().soft_drop)break;
        result=0;
    }while(false);
    platform_shutdown();if(!result)std::puts("SDL Down press/hold/OS-repeat/release/same-pump-tap/focus-loss and repeat gate passed; fixture only");return result;
}
