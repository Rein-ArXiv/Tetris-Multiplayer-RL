#ifndef TETRIS_STUDY_PLATFORM_H
#define TETRIS_STUDY_PLATFORM_H

#include <cstdint>

struct FrameInfo {
    double dt;
    unsigned events;
};

// All calls stay on the main thread that initialized SDL.
// SDL adds a current GL context; WIN32/SCRIPTED still implement input only.
// One active platform session. A real window backend owns its window.
// On failure, init releases its partial acquisition.
bool platform_init(int width, int height, const char* title);
// Idempotent: safe after a failed init or a previous shutdown.
void platform_shutdown() noexcept;
bool platform_should_close();
// Borrowed address for this session. Null for invalid name/inactive context.
// Non-null does not prove feature support; caller verifies the GL version.
void* platform_gl_get_proc(const char* name);
FrameInfo platform_begin_frame();
void platform_end_frame();

// Drawable pixels, not logical window units. Zero while inactive/minimized.
// Input-only backends return zero and cannot present GL content.
struct DrawableSize { int width = 0; int height = 0; };
DrawableSize platform_drawable_size();
// Present the completed default back buffer of this session's GL window.
// The session context must remain current. Does not issue drawing commands.
void platform_present();

enum class Key { Left, Right, Escape, Count };
// Queries are valid after begin_frame and before the next begin_frame.
// Unknown keys and calls outside an active lifetime return false.
bool platform_key_down(Key key);
bool platform_key_pressed(Key key);
bool platform_key_released(Key key);

// Consumes one ASCII byte; 0 means empty/inactive. Non-ASCII is unsupported.
char platform_get_char_pressed();
// Saturating count of rejected ASCII bytes due to a full queue, since init.
std::uint64_t platform_text_dropped();

#endif
