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

enum class Key { Left, Right, Escape, Count };
// Queries are valid after begin_frame and before the next begin_frame.
// Unknown keys and calls outside an active lifetime return false.
bool platform_key_down(Key key);
bool platform_key_pressed(Key key);
bool platform_key_released(Key key);

#endif
