#ifndef TETRIS_STUDY_PLATFORM_H
#define TETRIS_STUDY_PLATFORM_H

struct FrameInfo {
    double dt;
    unsigned events;
};

// One active window. On failure, init releases its partial acquisition.
bool platform_init(int width, int height, const char* title);
// Idempotent: safe after a failed init or a previous shutdown.
void platform_shutdown() noexcept;
bool platform_should_close();
FrameInfo platform_begin_frame();
void platform_end_frame();

#endif
