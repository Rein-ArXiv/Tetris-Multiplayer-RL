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
// SDL window screen coordinates; independent of drawable pixels.
struct WindowSize { int width = 0; int height = 0; };
struct WindowMouse { int x = 0; int y = 0; bool available = false; };
// Query after begin_frame on the main thread. Zero/unavailable if inactive,
// minimized, closing, or (for mouse) this window lacks mouse focus.
WindowSize platform_window_size();
WindowMouse platform_window_mouse();
// Swap request diagnostics: attempted is false for inactive/mismatched sessions
// or unsupported requested values. reported_interval is SDL's observation, NOT
// a measured display period; SDL may report 0 when the interval is unknown.
struct SwapIntervalReport {
    bool attempted = false;
    bool accepted = false;
    int reported_interval = 0;
};
// Only 0 and 1 are offered in this teaching API; no implicit adaptive fallback.
SwapIntervalReport platform_set_swap_interval(int requested);
// Returns true only if the swap API was called for this session's current
// context and a positive drawable. This is NOT display-completion evidence.
// SDL2 SwapWindow returns void, so true does not certify native swap success.
// Redraw the next frame; do not rely on preserved back-buffer contents.
bool platform_present();

enum class Key { Left, Right, Escape, Up, Down, Count };
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
