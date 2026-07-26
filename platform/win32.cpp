// platform/win32.cpp — Win32 window/input/timer + GDI framebuffer presentation

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>

#include "platform.h"

#include <cstdio>

static HWND s_hwnd = nullptr;
static HDC s_hdc = nullptr;
static HGLRC s_hglrc = nullptr;
static HMODULE s_opengl32 = nullptr;
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
    window_class.lpszClassName = "TetrisWindow";
    RegisterClassExA(&window_class);

    // SDL 쪽과 같은 조건을 준다 — 창 크기 조절 가능, 최대화 가능.
    // WS_THICKFRAME 이 없으면 논리 해상도만 바꿀 수 있고 창은 고정되어,
    // 같은 코드가 플랫폼마다 다르게 동작한다.
    const DWORD style = WS_OVERLAPPEDWINDOW;
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

    // OpenGL 3.3 Core 컨텍스트를 만든다. Windows 에서는 두 단계다.
    //
    // 1) PIXELFORMATDESCRIPTOR 로 "이 DC 에 GL 을 쓰겠다" 고 알리고
    // 2) 레거시 컨텍스트를 먼저 만든 뒤, 그 컨텍스트가 current 인 상태에서만
    //    조회 가능한 wglCreateContextAttribsARB 로 진짜 3.3 Core 를 만든다.
    //
    // 2단계를 건너뛰고 wglCreateContext 만 쓰면 드라이버 기본 호환 컨텍스트가
    // 나온다. 그러면 #version 330 core 셰이더가 통하는지 여부가 드라이버에
    // 따라 달라져 SDL 경로와 동작이 갈린다. 세 플랫폼이 같은 프로파일을
    // 받아야 셰이더를 한 벌만 유지할 수 있다.
    PIXELFORMATDESCRIPTOR pfd{};
    pfd.nSize      = sizeof(pfd);
    pfd.nVersion   = 1;
    pfd.dwFlags    = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;
    pfd.cAlphaBits = 8;
    const int pf = ChoosePixelFormat(s_hdc, &pfd);
    if (!pf || !SetPixelFormat(s_hdc, pf, &pfd)) {
        std::fprintf(stderr, "[GL] SetPixelFormat failed\n");
        s_should_close = true;
        return;
    }

    HGLRC legacy = wglCreateContext(s_hdc);
    if (!legacy) {
        std::fprintf(stderr, "[GL] wglCreateContext failed\n");
        s_should_close = true;
        return;
    }
    wglMakeCurrent(s_hdc, legacy);

    using CreateCtxAttribs = HGLRC (WINAPI*)(HDC, HGLRC, const int*);
    auto wglCreateContextAttribsARB = (CreateCtxAttribs)
        wglGetProcAddress("wglCreateContextAttribsARB");

    if (wglCreateContextAttribsARB) {
        const int attribs[] = {
            0x2091 /* MAJOR_VERSION */, 3,
            0x2092 /* MINOR_VERSION */, 3,
            0x9126 /* PROFILE_MASK  */, 0x00000001 /* CORE_PROFILE_BIT */,
            0
        };
        HGLRC core = wglCreateContextAttribsARB(s_hdc, nullptr, attribs);
        if (core) {
            wglMakeCurrent(nullptr, nullptr);
            wglDeleteContext(legacy);
            wglMakeCurrent(s_hdc, core);
            s_hglrc = core;
        } else {
            std::fprintf(stderr, "[GL] 3.3 Core unavailable; keeping legacy context\n");
            s_hglrc = legacy;
        }
    } else {
        std::fprintf(stderr, "[GL] wglCreateContextAttribsARB missing; legacy context\n");
        s_hglrc = legacy;
    }

    ShowWindow(s_hwnd, SW_SHOW);
    UpdateWindow(s_hwnd);
}

void platform_shutdown()
{
    // 컨텍스트를 DC 보다 먼저 놓는다. 순서를 바꾸면 이미 해제된 DC 를
    // 참조하는 상태로 wglDeleteContext 가 불린다.
    if (s_hglrc) {
        wglMakeCurrent(nullptr, nullptr);
        wglDeleteContext(s_hglrc);
        s_hglrc = nullptr;
    }
    if (s_opengl32) {
        FreeLibrary(s_opengl32);
        s_opengl32 = nullptr;
    }
    if (s_hdc && s_hwnd) {
        ReleaseDC(s_hwnd, s_hdc);
        s_hdc = nullptr;
    }
    if (s_hwnd) {
        DestroyWindow(s_hwnd);
        s_hwnd = nullptr;
    }
    UnregisterClassA("TetrisWindow", GetModuleHandleA(nullptr));
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

void platform_present()
{
    if (s_hdc) SwapBuffers(s_hdc);
}

void* platform_gl_get_proc(const char* name)
{
    // wglGetProcAddress 는 GL 1.2 이상만 돌려준다. glEnable 같은 1.1 함수는
    // NULL 이 나오므로 opengl32.dll 에서 직접 찾아야 한다. 이 폴백을
    // 빠뜨리면 로더가 "missing entry point: glEnable" 로 멈춘다.
    void* p = (void*)wglGetProcAddress(name);
    if (p == nullptr || p == (void*)0x1 || p == (void*)0x2 ||
        p == (void*)0x3 || p == (void*)-1) {
        if (!s_opengl32) s_opengl32 = LoadLibraryA("opengl32.dll");
        p = s_opengl32 ? (void*)GetProcAddress(s_opengl32, name) : nullptr;
    }
    return p;
}

void platform_viewport(int& x_out, int& y_out, int& w_out, int& h_out)
{
    // s_vp_* 는 창 좌상단 원점, GL 은 좌하단 원점.
    x_out = s_vp_x;
    y_out = s_win_h - s_vp_y - s_vp_h;
    w_out = s_vp_w;
    h_out = s_vp_h;
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

void platform_display_size(int& w_out, int& h_out)
{
    // SPI_GETWORKAREA 는 작업 표시줄을 뺀 영역이다. 실패하면 화면 전체.
    RECT work{};
    if (SystemParametersInfoA(SPI_GETWORKAREA, 0, &work, 0)) {
        w_out = work.right - work.left;
        h_out = work.bottom - work.top;
    } else {
        w_out = GetSystemMetrics(SM_CXSCREEN);
        h_out = GetSystemMetrics(SM_CYSCREEN);
    }
}

void platform_set_fullscreen(bool) {}
bool platform_fullscreen_supported() { return false; }
void platform_set_vsync(bool on) { s_frame_pacing = on; }
