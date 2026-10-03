// Exercise the production SDL event pump without opening an OpenGL window.
// Include the implementation in this test TU to set its timer baseline only;
// input is delivered through SDL's real queue and checked via the public API.
#include "../../platform/sdl.cpp"
#include <cstdlib>

static void require(bool value, const char* message)
{
    if (!value) { std::fprintf(stderr, "focus regression: %s\n", message); std::exit(1); }
}
static void push(SDL_Event event)
{
    require(SDL_PushEvent(&event) == 1, "SDL event enqueue");
}
int main()
{
    require(SDL_Init(SDL_INIT_EVENTS | SDL_INIT_TIMER) == 0, "SDL init");
    s_frequency = SDL_GetPerformanceFrequency();
    s_frame_start = SDL_GetPerformanceCounter();
    SDL_Event tap{}; tap.type=SDL_KEYDOWN; tap.key.keysym.sym=SDLK_LEFT;
    push(tap); tap.type=SDL_KEYUP; push(tap);
    platform_begin_frame();
    require(platform_key_pressed(PKEY_LEFT) && !platform_key_down(PKEY_LEFT), "same-pump tap");
    platform_begin_frame();
    require(!platform_key_pressed(PKEY_LEFT), "tap cleared next frame");
    tap.type=SDL_KEYDOWN; push(tap);
    SDL_Event cancel{}; cancel.type=SDL_WINDOWEVENT; cancel.window.event=SDL_WINDOWEVENT_FOCUS_LOST;
    push(cancel); tap.key.repeat=1; push(tap);
    platform_begin_frame();
    require(platform_input_cancelled() && !platform_key_pressed(PKEY_LEFT) && !platform_key_down(PKEY_LEFT), "cancel then repeat");
    platform_begin_frame();
    require(!platform_input_cancelled(), "cancel flag frame lifetime");
    push(cancel); tap.key.repeat=0; push(tap);
    platform_begin_frame();
    require(platform_input_cancelled() && platform_key_pressed(PKEY_LEFT), "fresh press after cancel");
    tap.type=SDL_KEYUP; push(tap); platform_begin_frame();

    SDL_Event quick{}; quick.type=SDL_MOUSEBUTTONDOWN; quick.button.button=SDL_BUTTON_LEFT;
    push(quick); quick.type=SDL_MOUSEBUTTONUP; push(quick); platform_begin_frame();
    require(platform_mouse_pressed(0)&&platform_mouse_released(0)&&!platform_mouse_down(0), "same-pump mouse down/up");
    platform_begin_frame();require(!platform_mouse_pressed(0)&&!platform_mouse_released(0), "mouse edges clear next frame");
    quick.type=SDL_MOUSEBUTTONDOWN;push(quick);push(cancel);platform_begin_frame();
    require(!platform_mouse_pressed(0)&&!platform_mouse_down(0)&&platform_mouse_released(0), "mouse press cancelled on focus loss");
    for(int bad : {-1,3,2147483647})require(!platform_mouse_pressed(bad)&&!platform_mouse_down(bad)&&!platform_mouse_released(bad), "invalid mouse button index");

    SDL_Event key{}; key.type = SDL_KEYDOWN; key.key.keysym.sym = SDLK_LEFT;
    SDL_Event mouse{}; mouse.type = SDL_MOUSEBUTTONDOWN; mouse.button.button = SDL_BUTTON_LEFT;
    push(key); push(mouse); platform_begin_frame();
    require(platform_key_down(PKEY_LEFT) && platform_key_pressed(PKEY_LEFT), "initial key down");
    require(platform_mouse_down(0) && platform_mouse_pressed(0), "initial mouse down");
    platform_begin_frame();
    require(platform_key_down(PKEY_LEFT) && !platform_key_pressed(PKEY_LEFT), "held key");
    SDL_Event lost{}; lost.type = SDL_WINDOWEVENT; lost.window.event = SDL_WINDOWEVENT_FOCUS_LOST;
    push(lost); platform_begin_frame();
    require(!platform_key_down(PKEY_LEFT), "focus loss must cancel key without key-up");
    require(!platform_mouse_down(0) && platform_mouse_released(0), "focus loss mouse release edge");
    platform_begin_frame();
    require(!platform_mouse_released(0), "release edge lasts one frame");
    SDL_Event gained = lost; gained.window.event = SDL_WINDOWEVENT_FOCUS_GAINED;
    push(gained); platform_begin_frame();
    require(!platform_key_down(PKEY_LEFT), "regaining focus must not revive stale keys");
    push(key); platform_begin_frame();
    require(platform_key_pressed(PKEY_LEFT), "fresh key after regaining focus");
    key.type = SDL_KEYUP; push(key); platform_begin_frame();
    require(!platform_key_down(PKEY_LEFT), "ordinary key release");
    s_logical_w=720; s_logical_h=640;
    s_vp_x=352; s_vp_y=25; s_vp_w=1215; s_vp_h=1080;
    s_mouse_x=351; s_mouse_y=24;
    require(platform_mouse_x()==-1 && platform_mouse_y()==-1, "letterbox outside remains negative");
    s_mouse_x=352; s_mouse_y=25;
    require(platform_mouse_x()==0 && platform_mouse_y()==0, "top-left edge included");
    s_mouse_x=1567; s_mouse_y=1105;
    require(platform_mouse_x()==720 && platform_mouse_y()==640, "far edges outside");
    s_vp_w=s_vp_h=0;
    require(platform_mouse_x()==-1 && platform_mouse_y()==-1, "invalid viewport sentinel");
    SDL_Quit();
    std::puts("Production SDL: focus events and letterbox mouse adapter boundaries passed");
}
