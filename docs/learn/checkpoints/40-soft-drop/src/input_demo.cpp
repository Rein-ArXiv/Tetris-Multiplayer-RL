#include "platform/platform.h"
#include <cstdio>
#include <cinttypes>

int main(int, char**)
{
    if (!platform_init(640, 480, "Tetris study: ASCII input")) return 1;

    double report_elapsed = 0.0;
    unsigned frames = 0;
    unsigned events = 0;
    while (!platform_should_close()) {
        const FrameInfo frame = platform_begin_frame();
        if (platform_should_close()) break;
        if (platform_key_pressed(Key::Escape)) break;
        if (platform_key_pressed(Key::Left)) std::puts("left pressed");
        if (platform_key_released(Key::Left)) std::puts("left released");
        while (const char ch = platform_get_char_pressed())
            std::printf("text byte=%u\n", static_cast<unsigned>(static_cast<unsigned char>(ch)));
        ++frames;
        events += frame.events;
        report_elapsed += frame.dt;
        if (report_elapsed >= 1.0) {
            std::printf("text dropped=%" PRIu64 "\n", platform_text_dropped());
            std::printf("left held=%d right held=%d\n",
                        platform_key_down(Key::Left), platform_key_down(Key::Right));
            std::printf("elapsed=%.3f frames=%u events=%u last_dt=%.6f\n",
                        report_elapsed, frames, events, frame.dt);
            report_elapsed = 0.0;
            frames = events = 0;
        }
        platform_end_frame();
    }
    platform_shutdown();
    return 0;
}
