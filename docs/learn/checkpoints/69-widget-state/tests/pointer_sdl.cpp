// SDL event queue and real platform pump, with controlled final cursor/focus
// queries and an input-only test session. GL initialization is not under test;
// the production platform still requires its double-buffered GL context.
#ifndef SDL_MAIN_HANDLED
#define SDL_MAIN_HANDLED
#endif
#include <SDL.h>
static bool focused=true;
static int cursor_x=500,cursor_y=400;
static SDL_Window* probe_window=nullptr;
static SDL_Window* probe_focus(){return focused?probe_window:nullptr;}
static Uint32 probe_cursor(int* x,int* y){*x=cursor_x;*y=cursor_y;return 0;}
#define SDL_GetMouseFocus probe_focus
#define SDL_GetMouseState probe_cursor
#include "platform/sdl.cpp"
#undef SDL_GetMouseFocus
#undef SDL_GetMouseState
#include "client/immediate_ui.h"
#include "client/application.h"
#include "simulation/state_hash.h"
#include <cstdlib>
#define CHECK(e) do{if(!(e)){std::fprintf(stderr,"CHECK %d: %s SDL=%s\n",__LINE__,#e,SDL_GetError());std::exit(1);}}while(false)
static void push(SDL_Event e){CHECK(SDL_PushEvent(&e)==1);}
int main(){
    CHECK(!platform_window_pointer().press);
    // Resources owns SDL; construct just the window/timer needed by the event
    // pump. A hidden dummy-driver window supplies a real SDL window ID.
    auto candidate=std::make_unique<Resources>();CHECK(candidate->runtime.ready());
    candidate->window.reset(SDL_CreateWindow("pointer event contract",0,0,800,600,SDL_WINDOW_HIDDEN));
    CHECK(candidate->window);
    candidate->frequency=SDL_GetPerformanceFrequency();CHECK(candidate->frequency);
    candidate->previous=candidate->frame_start=SDL_GetPerformanceCounter();
    active=std::move(candidate);probe_window=active->window.get();
    platform_begin_frame();
    const auto id=SDL_GetWindowID(probe_window);CHECK(id);
    SDL_Event e{};e.type=SDL_MOUSEBUTTONDOWN;e.button.windowID=id;e.button.button=SDL_BUTTON_LEFT;e.button.x=100;e.button.y=200;push(e);
    e.type=SDL_MOUSEBUTTONUP;push(e);
    platform_begin_frame();auto f=platform_window_pointer();
    CHECK(f.position&&f.position->x==500&&f.press&&f.press->x==100&&f.press->y==200&&!f.down);
    const auto layout=study_letterbox::make_layout({800,600},{1600,1200},{320,240});CHECK(layout);
    const auto button=study_ui::button({30,70,20,20},study_ui::map_pointer(layout,f));CHECK(button.activated&&!button.hovered);
    const auto initial=study_round::Round::create_seeded(study_grid::Grid{},1);CHECK(initial);
    study_app::Application app(*initial);
    auto changed=app.advance(.1,{},button.activated,false,true);CHECK(changed&&changed->game_replaced&&!changed->frame);
    CHECK(study_hash::state_hash(app.game()->round())==study_hash::state_hash(*initial));
    platform_begin_frame();CHECK(!platform_window_pointer().press);
    e.type=SDL_MOUSEBUTTONDOWN;e.button.windowID=id+1;push(e);platform_begin_frame();CHECK(!platform_window_pointer().press);
    e.button.windowID=id;e.button.button=SDL_BUTTON_RIGHT;push(e);platform_begin_frame();CHECK(!platform_window_pointer().press);
    e.button.button=SDL_BUTTON_LEFT;push(e);
    SDL_Event cancel{};cancel.type=SDL_WINDOWEVENT;cancel.window.windowID=id;cancel.window.event=SDL_WINDOWEVENT_FOCUS_LOST;push(cancel);
    platform_begin_frame();f=platform_window_pointer();CHECK(f.cancelled&&!f.down&&!f.press);
    platform_begin_frame();CHECK(!platform_window_pointer().press);
    push(e);cancel.window.event=SDL_WINDOWEVENT_LEAVE;push(cancel);platform_begin_frame();CHECK(platform_window_pointer().cancelled);
    platform_begin_frame();push(e);platform_begin_frame();CHECK(platform_window_pointer().press);
    focused=false;CHECK(platform_window_pointer().cancelled);
    focused=true;CHECK(platform_window_pointer().cancelled); // no resurrection within frame
    platform_begin_frame();CHECK(!platform_window_pointer().press&&!platform_window_pointer().down);
    e.type=SDL_MOUSEBUTTONUP;push(e);platform_begin_frame();
    SDL_Event quit{};quit.type=SDL_QUIT;push(quit);platform_begin_frame();CHECK(platform_window_pointer().cancelled);
    platform_shutdown();CHECK(!platform_window_pointer().press);
    std::puts("SDL pointer: native queue quick-click origin, foreign window/right button rejection, focus/leave cancellation, closing and menu transition without rule tick passed");
}
