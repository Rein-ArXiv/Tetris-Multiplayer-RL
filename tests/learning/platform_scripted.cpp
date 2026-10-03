#include "platform/platform.h"
#include <cassert>
int main()
{
    for (int cycle = 0; cycle < 2; ++cycle) {
        assert(platform_should_close());
        assert(platform_init(100, 100, "Scripted contract"));
        assert(!platform_init(100, 100, "Refuse duplicate"));
        for (int frame = 0; frame < 6; ++frame) {
            const auto info = platform_begin_frame();
            assert(info.dt == 0.25);
            const unsigned expected[] = {2,0,2,0,1,1};
            assert(info.events == expected[frame]);
            assert(platform_key_down(Key::Left) == (frame < 2));
            assert(platform_key_pressed(Key::Left) == (frame == 0));
            assert(platform_key_released(Key::Left) == (frame == 2));
            assert(platform_key_pressed(Key::Escape) == (frame == 4));
            assert(platform_should_close() == (frame == 5));
            if (frame == 0) assert(platform_get_char_pressed() == 'A');
            if (frame == 2) {
                assert(platform_get_char_pressed() == 'B');
                assert(platform_get_char_pressed() == 'C');
            }
            assert(platform_get_char_pressed() == 0);
            assert(platform_text_dropped() == 0);
            assert(!platform_key_down(Key::Count));
            assert(!platform_key_pressed(static_cast<Key>(-1)));
            platform_end_frame();
        }
        assert(platform_begin_frame().dt == 0);
        platform_shutdown(); platform_shutdown();
        assert(platform_should_close());
    }
}
