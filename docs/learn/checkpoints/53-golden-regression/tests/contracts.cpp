// Windows-only message injection, not a real keyboard/desktop test.
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include "platform/platform.h"
#include <cstdio>
#include <cstdlib>

static void require(bool condition, const char* label)
{
    if (!condition) { std::fprintf(stderr, "contract failed: %s\n", label); std::exit(1); }
}
static void post(HWND window, UINT message, WPARAM value = 0)
{
    require(PostMessageW(window, message, value, 1) != 0, "PostMessageW");
}
int main()
{
    require(!platform_init(0, 480, "invalid"), "invalid dimensions");
    require(!platform_init(640, 480, nullptr), "null title");
    require(!platform_init(640, 480, "\xff"), "invalid UTF-8 title");
    platform_shutdown();
    for (int cycle = 0; cycle < 2; ++cycle) {
        require(platform_init(640, 480, "Study contract"), "init/reinit");
        require(!platform_init(640, 480, "duplicate"), "duplicate init rejected");
        const HWND window = FindWindowW(L"TetrisStudyPlatformWin32Window", L"Study contract");
        require(window != nullptr, "window exists");
        platform_begin_frame();
        require(!platform_should_close(), "no stale quit after reinit");
        post(window, WM_KEYDOWN, VK_LEFT);
        platform_begin_frame();
        require(platform_key_pressed(Key::Left), "left edge");
        require(platform_key_pressed(Key::Left), "query does not consume");
        platform_begin_frame();
        require(platform_key_down(Key::Left) && !platform_key_pressed(Key::Left), "hold");
        post(window, WM_KILLFOCUS);
        platform_begin_frame();
        require(!platform_key_down(Key::Left) && platform_key_released(Key::Left), "focus cancel");
        platform_begin_frame();
        require(!platform_key_released(Key::Left), "release edge one frame");
        require(!platform_key_down(static_cast<Key>(-1)), "invalid key");
        post(window, WM_CHAR, 'A');
        post(window, WM_CHAR, 0xAC00); // UTF-16 Korean syllable excluded by ASCII contract.
        platform_begin_frame();
        require(platform_get_char_pressed() == 'A', "ASCII character");
        require(platform_get_char_pressed() == 0, "non-ASCII omitted");
        for (int i = 0; i < 64; ++i) post(window, WM_CHAR, 'B');
        platform_begin_frame();
        require(platform_text_dropped() == 1, "63-byte capacity");
        for (int i = 0; i < 63; ++i) require(platform_get_char_pressed() == 'B', "FIFO");
        require(platform_get_char_pressed() == 0, "queue empty");
        post(window, WM_CLOSE);
        platform_begin_frame();
        require(platform_should_close(), "close request");
        require(IsWindow(window) != 0, "deferred destruction until shutdown");
        platform_shutdown(); platform_shutdown();
        require(IsWindow(window) == 0, "window destroyed");
    }
    require(platform_init(640, 480, "Thread quit"), "third init");
    PostQuitMessage(7);
    platform_begin_frame();
    require(platform_should_close(), "WM_QUIT handled by pump");
    platform_shutdown();
    std::puts("Win32 injected-message contract passed; desktop input still needs manual checks.");
}
