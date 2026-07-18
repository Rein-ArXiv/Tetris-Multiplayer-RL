// platform/win32.cpp — Win32 window/input/timer + GDI framebuffer presentation

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>

#include "platform.h"

static HWND s_hwnd = nullptr;
static HDC s_hdc = nullptr;
static HDC s_present_dc = nullptr;
static HBITMAP s_present_bitmap = nullptr;
static HGDIOBJ s_present_old_bitmap = nullptr;
static int s_present_w = 0;
static int s_present_h = 0;
static bool s_should_close = false;
static bool s_frame_pacing = true;
static int s_win_w = 0;
static int s_win_h = 0;
static int s_logical_w = 0;
static int s_logical_h = 0;
static int s_vp_x = 0;
static int s_vp_y = 0;
static int s_vp_w = 0;
static int s_vp_h = 0;

static bool s_key_state[256]{};
static bool s_key_prev[256]{};
static char s_char_queue[64]{};
static int s_char_head = 0;
static int s_char_tail = 0;

static int s_mouse_x = 0;
static int s_mouse_y = 0;
static bool s_mouse_state[3]{};
static bool s_mouse_prev[3]{};
static float s_mouse_wheel = 0.0f;

static LARGE_INTEGER s_frequency{};
static LARGE_INTEGER s_init_time{};
static LARGE_INTEGER s_frame_start{};

static bool ensure_present_backbuffer(int width, int height)
{
    if (width <= 0 || height <= 0 || !s_hdc) return false;
    if (s_present_dc && s_present_bitmap &&
        s_present_w == width && s_present_h == height) return true;

    if (s_present_bitmap) {
        SelectObject(s_present_dc, s_present_old_bitmap);
        DeleteObject(s_present_bitmap);
        s_present_bitmap = nullptr;
    }
    if (!s_present_dc) s_present_dc = CreateCompatibleDC(s_hdc);
    if (!s_present_dc) return false;

    BITMAPINFO info{};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = width;
    info.bmiHeader.biHeight = -height;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    void* memory = nullptr;
    s_present_bitmap = CreateDIBSection(
        s_hdc, &info, DIB_RGB_COLORS, &memory, nullptr, 0);
    if (!s_present_bitmap) return false;
    s_present_old_bitmap = SelectObject(s_present_dc, s_present_bitmap);
    SetStretchBltMode(s_present_dc, COLORONCOLOR);
    s_present_w = width;
    s_present_h = height;
    return true;
}

static void recompute_viewport()
{
    if (s_win_w <= 0 || s_win_h <= 0 ||
        s_logical_w <= 0 || s_logical_h <= 0) {
        s_vp_x = s_vp_y = 0;
        s_vp_w = s_win_w;
        s_vp_h = s_win_h;
        return;
    }
    const double window_aspect = (double)s_win_w / (double)s_win_h;
    const double logical_aspect = (double)s_logical_w / (double)s_logical_h;
    if (window_aspect > logical_aspect) {
        s_vp_h = s_win_h;
        s_vp_w = (int)std::lround((double)s_win_h * logical_aspect);
        s_vp_x = (s_win_w - s_vp_w) / 2;
        s_vp_y = 0;
    } else {
        s_vp_w = s_win_w;
        s_vp_h = (int)std::lround((double)s_win_w / logical_aspect);
        s_vp_x = 0;
        s_vp_y = (s_win_h - s_vp_h) / 2;
    }
}

static LRESULT CALLBACK window_proc(HWND hwnd, UINT message,
                                    WPARAM wparam, LPARAM lparam)
{
    switch (message) {
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
        if (wparam < 256) s_key_state[wparam] = true;
        return 0;
    case WM_KEYUP:
    case WM_SYSKEYUP:
        if (wparam < 256) s_key_state[wparam] = false;
        return 0;
    case WM_CHAR:
        if (wparam > 0 && wparam < 128) {
            const int next = (s_char_tail + 1) % 64;
            if (next != s_char_head) {
                s_char_queue[s_char_tail] = (char)wparam;
                s_char_tail = next;
            }
        }
        return 0;
    case WM_SIZE:
        s_win_w = LOWORD(lparam);
        s_win_h = HIWORD(lparam);
        recompute_viewport();
        return 0;
    case WM_MOUSEMOVE:
        s_mouse_x = (int)(short)LOWORD(lparam);
        s_mouse_y = (int)(short)HIWORD(lparam);
        return 0;
    case WM_LBUTTONDOWN:
        s_mouse_state[0] = true; SetCapture(hwnd); return 0;
    case WM_LBUTTONUP:
        s_mouse_state[0] = false; ReleaseCapture(); return 0;
    case WM_RBUTTONDOWN:
        s_mouse_state[1] = true; SetCapture(hwnd); return 0;
    case WM_RBUTTONUP:
        s_mouse_state[1] = false; ReleaseCapture(); return 0;
    case WM_MBUTTONDOWN:
        s_mouse_state[2] = true; SetCapture(hwnd); return 0;
    case WM_MBUTTONUP:
        s_mouse_state[2] = false; ReleaseCapture(); return 0;
    case WM_MOUSEWHEEL:
        s_mouse_wheel += (float)(short)HIWORD(wparam) / (float)WHEEL_DELTA;
        return 0;
    case WM_ERASEBKGND:
        return 1;
    case WM_CLOSE:
        s_should_close = true;
        return 0;
    case WM_DESTROY:
        s_should_close = true;
        PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProcA(hwnd, message, wparam, lparam);
    }
}

void platform_init(int width, int height, const char* title)
{
    s_win_w = s_logical_w = width;
    s_win_h = s_logical_h = height;
    recompute_viewport();
    QueryPerformanceFrequency(&s_frequency);
    QueryPerformanceCounter(&s_init_time);
    s_frame_start = s_init_time;

    WNDCLASSEXA window_class{};
    window_class.cbSize = sizeof(window_class);
    window_class.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    window_class.lpfnWndProc = window_proc;
    window_class.hInstance = GetModuleHandleA(nullptr);
    window_class.hCursor = LoadCursor(nullptr, IDC_ARROW);
    window_class.lpszClassName = "TetrisSoftwareRenderer";
    RegisterClassExA(&window_class);

    const DWORD style =
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
    RECT rect{0, 0, width, height};
    AdjustWindowRect(&rect, style, FALSE);
    s_hwnd = CreateWindowExA(
        0, window_class.lpszClassName, title, style,
        CW_USEDEFAULT, CW_USEDEFAULT,
        rect.right - rect.left, rect.bottom - rect.top,
        nullptr, nullptr, window_class.hInstance, nullptr);
    if (!s_hwnd) {
        s_should_close = true;
        return;
    }
    s_hdc = GetDC(s_hwnd);
    SetStretchBltMode(s_hdc, COLORONCOLOR);
    ShowWindow(s_hwnd, SW_SHOW);
    UpdateWindow(s_hwnd);
}

void platform_shutdown()
{
    if (s_present_bitmap) {
        SelectObject(s_present_dc, s_present_old_bitmap);
        DeleteObject(s_present_bitmap);
        s_present_bitmap = nullptr;
        s_present_old_bitmap = nullptr;
    }
    if (s_present_dc) {
        DeleteDC(s_present_dc);
        s_present_dc = nullptr;
    }
    if (s_hdc && s_hwnd) {
        ReleaseDC(s_hwnd, s_hdc);
        s_hdc = nullptr;
    }
    if (s_hwnd) {
        DestroyWindow(s_hwnd);
        s_hwnd = nullptr;
    }
    UnregisterClassA("TetrisSoftwareRenderer", GetModuleHandleA(nullptr));
}

bool platform_should_close() { return s_should_close; }

float platform_begin_frame()
{
    std::memcpy(s_key_prev, s_key_state, sizeof(s_key_state));
    std::memcpy(s_mouse_prev, s_mouse_state, sizeof(s_mouse_state));
    s_mouse_wheel = 0.0f;

    MSG message;
    while (PeekMessageA(&message, nullptr, 0, 0, PM_REMOVE)) {
        if (message.message == WM_QUIT) s_should_close = true;
        TranslateMessage(&message);
        DispatchMessageA(&message);
    }

    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    float dt = (float)(now.QuadPart - s_frame_start.QuadPart) /
               (float)s_frequency.QuadPart;
    s_frame_start = now;
    return dt < 0.1f ? dt : 0.1f;
}

void platform_present(const uint32_t* pixels, int width, int height,
                      int pitch_bytes)
{
    if (!s_hdc || !pixels || width <= 0 || height <= 0) return;
    (void)pitch_bytes; // renderer surface is tightly packed
    if (!ensure_present_backbuffer(s_win_w, s_win_h)) return;

    RECT client{0, 0, s_win_w, s_win_h};
    FillRect(s_present_dc, &client, (HBRUSH)GetStockObject(BLACK_BRUSH));

    BITMAPINFO info{};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = width;
    info.bmiHeader.biHeight = -height; // top-down rows
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    StretchDIBits(s_present_dc,
                  s_vp_x, s_vp_y, s_vp_w, s_vp_h,
                  0, 0, width, height,
                  pixels, &info, DIB_RGB_COLORS, SRCCOPY);
    BitBlt(s_hdc, 0, 0, s_win_w, s_win_h, s_present_dc, 0, 0, SRCCOPY);
}

void platform_end_frame()
{
    if (!s_frame_pacing || s_frequency.QuadPart <= 0) return;
    const double target = 1.0 / 60.0;
    for (;;) {
        LARGE_INTEGER now;
        QueryPerformanceCounter(&now);
        const double elapsed =
            (double)(now.QuadPart - s_frame_start.QuadPart) /
            (double)s_frequency.QuadPart;
        const double remaining = target - elapsed;
        if (remaining <= 0.0) break;
        if (remaining > 0.002) Sleep((DWORD)((remaining - 0.001) * 1000.0));
        else Sleep(0);
    }
}

bool platform_key_pressed(int key)
{
    return key >= 0 && key < 256 && s_key_state[key] && !s_key_prev[key];
}

bool platform_key_down(int key)
{
    return key >= 0 && key < 256 && s_key_state[key];
}

char platform_get_char_pressed()
{
    if (s_char_head == s_char_tail) return 0;
    const char value = s_char_queue[s_char_head];
    s_char_head = (s_char_head + 1) % 64;
    return value;
}

int platform_mouse_x()
{
    if (s_vp_w <= 0) return s_mouse_x;
    return (int)((double)(s_mouse_x - s_vp_x) * s_logical_w / s_vp_w);
}

int platform_mouse_y()
{
    if (s_vp_h <= 0) return s_mouse_y;
    return (int)((double)(s_mouse_y - s_vp_y) * s_logical_h / s_vp_h);
}

bool platform_mouse_pressed(int button)
{
    return button >= 0 && button < 3 &&
           s_mouse_state[button] && !s_mouse_prev[button];
}

bool platform_mouse_down(int button)
{
    return button >= 0 && button < 3 && s_mouse_state[button];
}

bool platform_mouse_released(int button)
{
    return button >= 0 && button < 3 &&
           !s_mouse_state[button] && s_mouse_prev[button];
}

float platform_mouse_wheel() { return s_mouse_wheel; }

double platform_get_time()
{
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    return (double)(now.QuadPart - s_init_time.QuadPart) /
           (double)s_frequency.QuadPart;
}

void platform_set_window_size(int width, int height)
{
    if (!s_hwnd || width <= 0 || height <= 0) return;
    RECT rect{0, 0, width, height};
    const DWORD style = (DWORD)GetWindowLongPtr(s_hwnd, GWL_STYLE);
    AdjustWindowRect(&rect, style, FALSE);
    SetWindowPos(s_hwnd, nullptr, 0, 0,
                 rect.right - rect.left, rect.bottom - rect.top,
                 SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    s_win_w = width;
    s_win_h = height;
    recompute_viewport();
}

void platform_set_fullscreen(bool) {}
bool platform_fullscreen_supported() { return false; }
void platform_set_vsync(bool on) { s_frame_pacing = on; }
