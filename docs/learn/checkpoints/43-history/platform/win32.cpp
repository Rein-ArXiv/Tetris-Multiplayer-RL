// platform/win32.cpp
//
// Independent Win32 backend for the Tetris study platform layer.
// Console executable, user32 only, ASCII text contract; no GL/mouse/audio/DPI.
// Source and main stay unchanged: this file only fills the platform API.

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include "platform/platform.h"
#include "platform/text_queue.h"

#include <cstdint>
#include <cstdio>
#include <memory>
#include <exception>
#include <utility>
#include <string>

namespace {

constexpr int kMaxDimension = 16384;        // Disclosed safe upper bound.
constexpr std::size_t kTextSlots = 64;      // AsciiQueue<64>: one slot is a sentinel.
constexpr double kTargetFrameSeconds = 1.0 / 60.0;

// Stable private class name owned by this module. We never adopt a foreign class.
const wchar_t kWindowClassName[] = L"TetrisStudyPlatformWin32Window";

struct Resources {
    HWND hwnd = nullptr;
    HINSTANCE instance = nullptr;
    bool class_registered = false;

    bool should_close = false;

    AsciiQueue<kTextSlots> text{};
    std::uint64_t dropped = 0;

    bool current[static_cast<int>(Key::Count)] = {};
    bool previous[static_cast<int>(Key::Count)] = {};

    LARGE_INTEGER frequency{};
    LARGE_INTEGER previous_counter{};
    LARGE_INTEGER frame_start{};

    ~Resources() noexcept
    {
        // Idempotent cleanup: window first, then class registration.
        if (hwnd != nullptr) {
            DestroyWindow(hwnd);
            hwnd = nullptr;
        }
        if (class_registered) {
            UnregisterClassW(kWindowClassName, instance);
            class_registered = false;
        }
    }
};

// Exactly one active session. Kept alive by a unique_ptr so the address is stable
// and the destructor drives cleanup even on partial failure.
std::unique_ptr<Resources> active;

Resources* GetResources(HWND hwnd) noexcept
{
    return reinterpret_cast<Resources*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
}

int KeySlot(Key key) noexcept
{
    const int slot = static_cast<int>(key);
    if (slot < 0 || slot >= static_cast<int>(Key::Count)) return -1;
    return slot;
}

// Control keymap only: Left, Right, Escape. Everything else is ignored.
void SetKeyState(Resources& res, WPARAM vk, bool down) noexcept
{
    Key key;
    switch (vk) {
    case VK_LEFT:   key = Key::Left;   break;
    case VK_RIGHT:  key = Key::Right;  break;
    case VK_ESCAPE: key = Key::Escape; break;
    case VK_UP:     key = Key::Up;     break;
    case VK_DOWN:   key = Key::Down;   break;
    case VK_SPACE:  key = Key::Space;  break;
    default: return;
    }
    res.current[static_cast<int>(key)] = down;
}

bool ReadCounter(LARGE_INTEGER& counter) noexcept
{
    return QueryPerformanceCounter(&counter) != 0;
}

// The window procedure never lets a C++ exception cross the OS boundary.
LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) noexcept
{
    switch (message) {
    case WM_NCCREATE: {
        const auto* create = reinterpret_cast<const CREATESTRUCTW*>(lParam);
        auto* res = static_cast<Resources*>(create->lpCreateParams);
        if (res == nullptr) return FALSE;
        // Synchronous creation messages arrive before active is published;
        // GWLP_USERDATA is the only route back to Resources at this point.
        SetLastError(0);
        const LONG_PTR previous = SetWindowLongPtrW(
            hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(res));
        if (previous == 0 && GetLastError() != 0) {
            std::fprintf(stderr,
                "WindowProc: SetWindowLongPtrW(GWLP_USERDATA) failed (%lu)\n",
                static_cast<unsigned long>(GetLastError()));
            return FALSE; // Abort window creation rather than run without state.
        }
        res->hwnd = hwnd;
        return TRUE;
    }
    case WM_NCDESTROY: {
        auto* res = GetResources(hwnd);
        if (res != nullptr) {
            res->hwnd = nullptr;
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
        }
        // Keep default handling for the non-client teardown.
        return DefWindowProcW(hwnd, message, wParam, lParam);
    }
    case WM_CLOSE: {
        auto* res = GetResources(hwnd);
        if (res != nullptr) res->should_close = true;
        return 0; // Actual destruction happens at shutdown, not here.
    }
    case WM_DESTROY: {
        auto* res = GetResources(hwnd);
        if (res != nullptr) res->should_close = true;
        // Deliberately no PostQuitMessage. A reusable platform layer must not
        // enqueue a thread-quit that would outlive this session and surprise a
        // later reinit; the caller owns the message-loop policy. Standalone
        // Win32 tutorials post the quit here precisely because they own the loop.
        return 0;
    }
    case WM_KEYDOWN: {
        auto* res = GetResources(hwnd);
        if (res != nullptr) SetKeyState(*res, wParam, true);
        return 0;
    }
    case WM_KEYUP: {
        auto* res = GetResources(hwnd);
        if (res != nullptr) SetKeyState(*res, wParam, false);
        return 0;
    }
    case WM_SYSKEYDOWN: {
        auto* res = GetResources(hwnd);
        if (res != nullptr) SetKeyState(*res, wParam, true);
        // Delegate so Alt+F4 and other default system shortcuts still work.
        return DefWindowProcW(hwnd, message, wParam, lParam);
    }
    case WM_SYSKEYUP: {
        auto* res = GetResources(hwnd);
        if (res != nullptr) SetKeyState(*res, wParam, false);
        return DefWindowProcW(hwnd, message, wParam, lParam);
    }
    case WM_CHAR: {
        auto* res = GetResources(hwnd);
        // ASCII contract only: accept 1..127, discard other UTF-16 code units.
        // ASCII control characters are included; this is not a text editor.
        if (res != nullptr && wParam >= 1 && wParam <= 127) {
            if (!res->text.push(static_cast<unsigned char>(wParam))) {
                if (res->dropped != UINT64_MAX) ++res->dropped; // Saturating.
            }
        }
        return 0;
    }
    case WM_KILLFOCUS: {
        auto* res = GetResources(hwnd);
        if (res != nullptr) {
            // Focus loss clears the current snapshot so held keys will report
            // released when observed after the event pump.
            for (int i = 0; i < static_cast<int>(Key::Count); ++i)
                res->current[i] = false;
        }
        return 0;
    }
    default:
        // Includes WM_PAINT, handled with the class background brush.
        return DefWindowProcW(hwnd, message, wParam, lParam);
    }
}

bool ConvertTitle(const char* title, std::wstring& out)
{
    // First call asks for the length including the terminating NUL.
    const int length = MultiByteToWideChar(
        CP_UTF8, MB_ERR_INVALID_CHARS, title, -1, nullptr, 0);
    if (length <= 0) return false;
    out.assign(static_cast<std::size_t>(length), L'\0');
    const int written = MultiByteToWideChar(
        CP_UTF8, MB_ERR_INVALID_CHARS, title, -1, out.data(), length);
    if (written != length) return false;
    out.resize(static_cast<std::size_t>(length) - 1); // Drop the trailing NUL.
    return true;
}

} // namespace

bool platform_init(int width, int height, const char* title)
{
    if (active) {
        std::fprintf(stderr, "platform_init: a session is already active\n");
        return false; // Duplicate init rejected before any acquisition.
    }
    if (title == nullptr) {
        std::fprintf(stderr, "platform_init: title must not be null\n");
        return false;
    }
    if (width <= 0 || height <= 0) {
        std::fprintf(stderr, "platform_init: width and height must be positive\n");
        return false;
    }
    if (width > kMaxDimension || height > kMaxDimension) {
        std::fprintf(stderr, "platform_init: dimensions must not exceed %d\n",
                     kMaxDimension);
        return false; // Reject large/overflowing sizes before any arithmetic.
    }

    try {
        auto candidate = std::make_unique<Resources>();

        std::wstring wide_title;
        if (!ConvertTitle(title, wide_title)) {
            std::fprintf(stderr, "platform_init: title is not valid UTF-8\n");
            return false; // candidate destructor performs no acquisition yet.
        }

        LARGE_INTEGER frequency;
        if (!QueryPerformanceFrequency(&frequency) || frequency.QuadPart <= 0) {
            std::fprintf(stderr, "platform_init: QueryPerformanceFrequency failed (%lu)\n",
                         static_cast<unsigned long>(GetLastError()));
            return false;
        }
        candidate->frequency = frequency;
        if (!ReadCounter(candidate->previous_counter)) {
            std::fputs("initial QueryPerformanceCounter failed\n", stderr);
            return false;
        }
        candidate->frame_start = candidate->previous_counter;

        const HINSTANCE instance = GetModuleHandleW(nullptr);
        if (instance == nullptr) {
            std::fprintf(stderr, "platform_init: GetModuleHandleW failed (%lu)\n",
                         static_cast<unsigned long>(GetLastError()));
            return false;
        }
        candidate->instance = instance;

        WNDCLASSEXW window_class{};
        window_class.cbSize = sizeof(window_class);
        window_class.style = CS_HREDRAW | CS_VREDRAW;
        window_class.lpfnWndProc = &WindowProc;
        window_class.hInstance = instance;
        window_class.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
        window_class.hbrBackground =
            reinterpret_cast<HBRUSH>(static_cast<INT_PTR>(COLOR_WINDOW + 1));
        window_class.lpszClassName = kWindowClassName;
        if (RegisterClassExW(&window_class) == 0) {
            // If this fails because the name already exists, the class is not ours;
            // fail instead of silently driving somebody else's window class.
            std::fprintf(stderr, "platform_init: RegisterClassExW failed (%lu)\n",
                         static_cast<unsigned long>(GetLastError()));
            return false;
        }
        candidate->class_registered = true;

        RECT rect{0, 0, width, height};
        if (!AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE)) {
            std::fprintf(stderr, "platform_init: AdjustWindowRect failed (%lu)\n",
                         static_cast<unsigned long>(GetLastError()));
            return false;
        }

        // CreateWindowExW dispatches synchronous messages into WindowProc before we
        // return; candidate state already exists and its address is stable.
        const HWND hwnd = CreateWindowExW(
            0, kWindowClassName, wide_title.c_str(), WS_OVERLAPPEDWINDOW,
            CW_USEDEFAULT, CW_USEDEFAULT,
            rect.right - rect.left, rect.bottom - rect.top,
            nullptr, nullptr, instance, candidate.get());
        if (hwnd == nullptr) {
            std::fprintf(stderr, "platform_init: CreateWindowExW failed (%lu)\n",
                         static_cast<unsigned long>(GetLastError()));
            return false;
        }
        candidate->hwnd = hwnd; // WM_NCCREATE already recorded it; keep it explicit.

        // ShowWindow/UpdateWindow only after resources exist.
        ShowWindow(hwnd, SW_SHOW);
        UpdateWindow(hwnd);

        active = std::move(candidate); // Publish last; pointer address stays stable.
        return true;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "initialization exception: %s\n", error.what());
        return false;
    }
}

void platform_shutdown() noexcept
{
    active.reset(); // The owner destroys the window while callback state is alive.
}

bool platform_should_close()
{
    if (!active) return true;
    return active->should_close;
}

FrameInfo platform_begin_frame()
{
    FrameInfo info{0.0, 0u};
    if (!active) return info;
    Resources& res = *active;

    // Snapshot current into previous BEFORE pumping: PeekMessage can synchronously
    // dispatch sent messages and mutate current during the same thread turn.
    for (int i = 0; i < static_cast<int>(Key::Count); ++i)
        res.previous[i] = res.current[i];

    if (!ReadCounter(res.frame_start)) {
        std::fputs("frame QueryPerformanceCounter failed\n", stderr);
        res.should_close = true;
        return info;
    }
    info.dt = static_cast<double>(res.frame_start.QuadPart - res.previous_counter.QuadPart)
            / static_cast<double>(res.frequency.QuadPart);
    res.previous_counter = res.frame_start;

    MSG message;
    while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
        ++info.events; // Retrieved messages, not all synchronous callbacks.
        if (message.message == WM_QUIT) {
            // Thread quit belongs to the caller's loop policy; record and stop.
            // Never Dispatch it, and never assume events == callback count.
            res.should_close = true;
            break;
        }
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    return info;
}

void platform_end_frame()
{
    if (!active || active->should_close) return;
    Resources& res = *active;
    LARGE_INTEGER now{};
    if (!ReadCounter(now)) {
        std::fputs("end QueryPerformanceCounter failed\n", stderr);
        res.should_close = true;
        return;
    }
    const double work = static_cast<double>(now.QuadPart - res.frame_start.QuadPart)
                      / static_cast<double>(res.frequency.QuadPart);
    const double remaining = kTargetFrameSeconds - work;
    if (remaining >= 0.001) Sleep(static_cast<DWORD>(remaining * 1000.0));
}

bool platform_key_down(Key key)
{
    if (!active) return false;
    const int slot = KeySlot(key);
    if (slot < 0) return false;
    return active->current[slot];
}

bool platform_key_pressed(Key key)
{
    if (!active) return false;
    const int slot = KeySlot(key);
    if (slot < 0) return false;
    return active->current[slot] && !active->previous[slot];
}

bool platform_key_released(Key key)
{
    if (!active) return false;
    const int slot = KeySlot(key);
    if (slot < 0) return false;
    return active->previous[slot] && !active->current[slot];
}

char platform_get_char_pressed()
{
    if (!active) return 0;
    return active->text.pop();
}

std::uint64_t platform_text_dropped()
{
    if (!active) return 0;
    return active->dropped;
}

// Input-only teaching backend: no GL context to supply addresses for.
void* platform_gl_get_proc(const char*) { return nullptr; }

DrawableSize platform_drawable_size() { return {}; }
SwapIntervalReport platform_set_swap_interval(int) { return {}; }
bool platform_present() { return false; }

WindowSize platform_window_size() { return {}; }
WindowMouse platform_window_mouse() { return {}; }
