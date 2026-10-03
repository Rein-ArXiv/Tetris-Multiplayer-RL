#include "platform/platform.h"
#include <SDL.h>
#include <cassert>
#include <cstring>
#include <initializer_list>
#ifdef LEARN_TEXT
#include "platform/text_queue.h"
#endif

void key(Uint32 type, SDL_Keycode code, Uint8 repeat = 0)
{
    SDL_Event event{};
    event.type = type;
    event.key.keysym.sym = code;
    event.key.repeat = repeat;
    assert(SDL_PushEvent(&event) == 1);
}
void left(bool down, bool pressed, bool released)
{
    assert(platform_key_down(Key::Left) == down);
    assert(platform_key_pressed(Key::Left) == pressed);
    assert(platform_key_released(Key::Left) == released);
}
#ifdef LEARN_TEXT
void text(const char* value)
{
    SDL_Event event{};
    event.type = SDL_TEXTINPUT;
    assert(std::strlen(value) < sizeof(event.text.text));
    std::strcpy(event.text.text, value);
    assert(SDL_PushEvent(&event) == 1);
}
void queue_tests()
{
    AsciiQueue<4> q;
    assert(!q.push(0) && !q.push(128));
    assert(q.pop() == 0);
    assert(q.push('A') && q.push('B') && q.push('C'));
    assert(!q.push('D'));
    assert(q.pop() == 'A' && q.push('D')); // tail wraps to 0.
    assert(q.pop() == 'B' && q.pop() == 'C' && q.pop() == 'D' && q.pop() == 0);
    AsciiQueue<64> full;
    for (unsigned n = 0; n < 300; ++n) {
        for (unsigned i = 1; i <= 63; ++i) assert(full.push(i));
        assert(!full.push('Z'));
        for (unsigned i = 1; i <= 63; ++i) assert(full.pop() == i);
        assert(full.pop() == 0);
    }
}
#endif
int main()
{
    left(false, false, false);
    assert(platform_init(100, 100, "Input contract test"));
    platform_begin_frame();
    left(false, false, false);
    key(SDL_KEYDOWN, SDLK_LEFT);
    platform_begin_frame(); left(true, true, false);
    assert(platform_key_pressed(Key::Left)); // Query does not consume the edge.
    platform_begin_frame(); left(true, false, false);
    key(SDL_KEYDOWN, SDLK_LEFT, 1);
    platform_begin_frame(); left(true, false, false);
    key(SDL_KEYUP, SDLK_LEFT);
    platform_begin_frame(); left(false, false, true);
    platform_begin_frame(); left(false, false, false);
    key(SDL_KEYDOWN, SDLK_LEFT); key(SDL_KEYUP, SDLK_LEFT);
    platform_begin_frame(); left(false, false, false); // Endpoint sampling limitation.
    key(SDL_KEYDOWN, SDLK_LEFT);
    platform_begin_frame(); left(true, true, false);
    SDL_Event lost{}; lost.type = SDL_WINDOWEVENT; lost.window.event = SDL_WINDOWEVENT_FOCUS_LOST;
    assert(SDL_PushEvent(&lost) == 1);
    platform_begin_frame(); left(false, false, true);
    key(SDL_KEYDOWN, SDLK_a);
    platform_begin_frame(); left(false, false, false);
    for (Key unknown : {Key::Count, static_cast<Key>(-1)}) {
        assert(!platform_key_down(unknown));
        assert(!platform_key_pressed(unknown));
        assert(!platform_key_released(unknown));
    }
#ifdef LEARN_TEXT
    queue_tests();
    text("AB"); text("\xED\x95\x9C"); text("C");
    platform_begin_frame();
    assert(platform_get_char_pressed() == 'A');
    platform_begin_frame(); // Unconsumed text survives frames.
    assert(platform_get_char_pressed() == 'B');
    assert(platform_get_char_pressed() == 'C');
    assert(platform_get_char_pressed() == 0 && platform_text_dropped() == 0);
    for (int i = 0; i < 4; ++i) text("1234567890123456"); // 64 ASCII bytes.
    platform_begin_frame();
    assert(platform_text_dropped() == 1);
    for (int i = 0; i < 63; ++i) assert(platform_get_char_pressed() == "1234567890123456"[i % 16]);
    assert(platform_get_char_pressed() == 0);
#endif
    platform_shutdown(); left(false, false, false);
#ifdef LEARN_TEXT
    assert(platform_get_char_pressed() == 0 && platform_text_dropped() == 0);
#endif
    assert(platform_init(100,100,"Fresh input"));
    platform_begin_frame(); left(false,false,false);
#ifdef LEARN_TEXT
    assert(platform_get_char_pressed() == 0 && platform_text_dropped() == 0);
#endif
    platform_shutdown();
}
