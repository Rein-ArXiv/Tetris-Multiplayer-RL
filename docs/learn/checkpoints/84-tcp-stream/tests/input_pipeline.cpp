#include "platform/sdl.cpp"
#include "loop/frame_runner.h"
#include <cstdlib>
#define CHECK(x) do { if (!(x)) { std::fprintf(stderr,"CHECK failed line %d: %s\n",__LINE__,#x); std::exit(1); } } while(false)
static void key(Uint32 type, SDL_Keycode code, Uint8 repeat=0) {
    SDL_Event e{}; e.type=type; e.key.keysym.sym=code; e.key.repeat=repeat;
    CHECK(SDL_PushEvent(&e)==1);
}
static void cancel() {
    SDL_Event e{};e.type=SDL_WINDOWEVENT;e.window.event=SDL_WINDOWEVENT_FOCUS_LOST;
    CHECK(SDL_PushEvent(&e)==1);
}
static study_loop::FrameInput sample() {
    platform_begin_frame();
    return {platform_key_pressed(Key::Left),platform_key_pressed(Key::Right),
        platform_key_pressed(Key::Up),platform_key_down(Key::Down),
        platform_key_pressed(Key::Space),platform_input_cancelled()};
}
int main() {
    active=std::make_unique<Resources>();CHECK(active->runtime.ready());
    active->frequency=SDL_GetPerformanceFrequency();CHECK(active->frequency);
    active->previous=active->frame_start=SDL_GetPerformanceCounter();
    const auto round=study_round::Round::create({},study_catalog::Kind::T);CHECK(round);
    study_loop::FrameRunner runner(*round);
    key(SDL_KEYDOWN,SDLK_UP);key(SDL_KEYUP,SDLK_UP);
    auto raw=sample();CHECK(raw.up&&!platform_key_down(Key::Up)&&platform_key_released(Key::Up));
    auto zero=runner.advance(0,raw);CHECK(zero&&zero->ticks==0);
    for(int i=0;i<3;++i){zero=runner.advance(0,sample());CHECK(zero&&zero->ticks==0);}
    auto one=runner.advance(.017,sample());CHECK(one&&one->ticks==1&&runner.round().quarter()==1);
    auto more=runner.advance(.05,sample());CHECK(more&&more->ticks==3&&runner.round().quarter()==1);
    // A pending drop must not run after focus loss, including a zero-tick cancel frame.
    key(SDL_KEYDOWN,SDLK_SPACE);key(SDL_KEYUP,SDLK_SPACE);
    CHECK(runner.advance(0,sample())); cancel();raw=sample();CHECK(raw.cancelled);
    CHECK(runner.advance(0,raw));
    one=runner.advance(.017,sample());CHECK(one&&one->ticks==1&&!one->board_changed);
    // Cancel old rotation, then accept a fresh drop later in the same pump.
    key(SDL_KEYDOWN,SDLK_UP);key(SDL_KEYUP,SDLK_UP);CHECK(runner.advance(0,sample()));
    cancel();key(SDL_KEYDOWN,SDLK_SPACE,1);raw=sample();CHECK(raw.cancelled&&!raw.drop);
    CHECK(runner.advance(0,raw));
    cancel();key(SDL_KEYDOWN,SDLK_SPACE);key(SDL_KEYUP,SDLK_SPACE);
    raw=sample();CHECK(raw.cancelled&&raw.drop);
    more=runner.advance(.05,raw);CHECK(more&&more->ticks==3&&more->observations[0].lock);
    CHECK(!more->observations[1].lock&&!more->observations[2].lock);
    // Down stays level-based: a completed tap ends released.
    key(SDL_KEYDOWN,SDLK_DOWN);key(SDL_KEYUP,SDLK_DOWN);raw=sample();
    CHECK(platform_key_pressed(Key::Down)&&!raw.down);
    platform_shutdown();CHECK(!platform_input_cancelled());
    std::puts("SDL-to-frame-to-tick: tap, zero ticks, consume, cancellation, repeat, fresh press passed");
}
