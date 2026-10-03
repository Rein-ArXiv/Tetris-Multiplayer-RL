#include "platform/platform.h"
#include <cstdio>

int main(int, char**)
{
    if (!platform_init(640, 480, "Tetris study: modules")) return 1;

    double report_elapsed = 0.0;
    unsigned frames = 0;
    unsigned events = 0;
    while (!platform_should_close()) {
        const FrameInfo frame = platform_begin_frame();
        if (platform_should_close()) break;
        ++frames;
        events += frame.events;
        report_elapsed += frame.dt;
        if (report_elapsed >= 1.0) {
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
