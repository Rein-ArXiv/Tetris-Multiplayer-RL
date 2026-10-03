// platform/sdl.cpp — SDL2 창/입력/타이머 + OpenGL 3.3 Core 컨텍스트

#include <SDL2/SDL.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

#include "platform.h"
#include "../core/key_edges.h"
#include "mouse_coordinates.h"

#ifdef __APPLE__
#include <unistd.h>
#endif

static SDL_Window* s_window = nullptr;
static bool s_should_close = false;
static bool s_frame_pacing = true;
static bool s_fullscreen = false;
static int s_win_w = 0;
static int s_win_h = 0;
static int s_logical_w = 0;
static int s_logical_h = 0;
static SDL_GLContext s_glctx = nullptr;

static int s_vp_x = 0;
static int s_vp_y = 0;
static int s_vp_w = 0;
static int s_vp_h = 0;

static input_detail::KeyEdges<256> s_keys;
static char s_char_queue[64]{};
static int s_char_head = 0;
static int s_char_tail = 0;

static int s_mouse_x = 0;
static int s_mouse_y = 0;
static input_detail::KeyEdges<3> s_mouse;
static float s_mouse_wheel = 0.0f;

static uint64_t s_frequency = 1;
static uint64_t s_init_time = 0;
static uint64_t s_frame_start = 0;

#ifdef __APPLE__
static void set_macos_resource_cwd()
{
    char* base = SDL_GetBasePath();
    if (!base) return;
    const std::string resources = std::string(base) + "../Resources";
    SDL_free(base);
    if (access(resources.c_str(), R_OK) == 0 &&
        chdir(resources.c_str()) != 0) {
        std::fprintf(stderr, "[SDL] resource cwd failed: %s\n",
                     resources.c_str());
    }
}
#endif

static int sdl_to_platform_key(SDL_Keycode key)
{
    switch (key) {
    case SDLK_LEFT: return PKEY_LEFT;
    case SDLK_RIGHT: return PKEY_RIGHT;
    case SDLK_UP: return PKEY_UP;
    case SDLK_DOWN: return PKEY_DOWN;
    case SDLK_SPACE: return PKEY_SPACE;
    case SDLK_RETURN:
    case SDLK_KP_ENTER: return PKEY_ENTER;
    case SDLK_ESCAPE: return PKEY_ESCAPE;
    case SDLK_BACKSPACE: return PKEY_BACK;
    case SDLK_q: return PKEY_Q;
    case SDLK_r: return PKEY_R;
    case SDLK_h: return PKEY_H;
    case SDLK_p: return PKEY_P;
    case SDLK_c: return PKEY_C;
    case SDLK_j: return PKEY_J;
    case SDLK_t: return PKEY_T;
    case SDLK_y: return PKEY_Y;
    case SDLK_n: return PKEY_N;
    case SDLK_LEFTBRACKET: return PKEY_LBRACKET;
    case SDLK_RIGHTBRACKET: return PKEY_RBRACKET;
    case SDLK_F5: return PKEY_F5;
    case SDLK_F6: return PKEY_F6;
    default: return -1;
    }
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

void platform_init(int width, int height, const char* title)
{
    s_keys.reset();
    s_mouse.reset();
    s_win_w = s_logical_w = width;
    s_win_h = s_logical_h = height;
    recompute_viewport();
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
        std::fprintf(stderr, "[SDL] init failed: %s\n", SDL_GetError());
        s_should_close = true;
        return;
    }
#ifdef __APPLE__
    set_macos_resource_cwd();
#endif
    // OpenGL 3.3 Core 를 명시적으로 요청한다. 세 플랫폼 모두 같은 프로파일을
    // 받도록 맞춰야 셰이더(#version 330 core)가 전제하는 컨텍스트와 어긋나지
    // 않는다. macOS 는 Core 프로파일이 아니면 3.x 자체를 주지 않으므로 이
    // 설정이 필수다.
    if (SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE) != 0 ||
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3) != 0 ||
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3) != 0 ||
        SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1) != 0) {
        std::fprintf(stderr, "[SDL] GL attribute setup failed: %s\n", SDL_GetError());
        s_should_close = true;
        return;
    }

    s_window = SDL_CreateWindow(
        title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        width, height, SDL_WINDOW_SHOWN | SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    if (!s_window) {
        std::fprintf(stderr, "[SDL] window creation failed: %s\n", SDL_GetError());
        s_should_close = true;
        return;
    }

    s_glctx = SDL_GL_CreateContext(s_window);
    if (!s_glctx) {
        std::fprintf(stderr, "[SDL] GL 3.3 Core context failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(s_window);
        s_window = nullptr;
        s_should_close = true;
        return;
    }
    if (SDL_GL_MakeCurrent(s_window, s_glctx) != 0) {
        std::fprintf(stderr, "[SDL] GL make current failed: %s\n", SDL_GetError());
        // 창/컨텍스트는 여기서 파괴하지 않고 남긴다. 호출자는 실패해도 항상
        // platform_shutdown 을 호출하므로 정리는 거기서 이뤄진다.
        s_should_close = true;
        return;
    }
    if (SDL_GL_SetSwapInterval(s_frame_pacing ? 1 : 0) != 0) {
        // 비치명적 경고. 페이싱은 sleep 폴백이 있으므로 s_frame_pacing 정책은
        // 그대로 두고 실패를 알리기만 한다.
        std::fprintf(stderr, "[SDL] swap interval unavailable (nonfatal): %s\n", SDL_GetError());
    }
    s_frequency = SDL_GetPerformanceFrequency();
    if (s_frequency == 0) {
        std::fprintf(stderr, "[SDL] performance frequency unavailable\n");
        s_should_close = true;
        return;
    }
    SDL_StartTextInput();
    s_init_time = SDL_GetPerformanceCounter();
    s_frame_start = s_init_time;
}

void platform_shutdown()
{
    SDL_StopTextInput();
    // 컨텍스트를 창보다 먼저 지운다 — 창이 사라진 뒤 GL 자원을 만지면 안 된다.
    if (s_glctx) {
        SDL_GL_DeleteContext(s_glctx);
        s_glctx = nullptr;
    }
    if (s_window) {
        SDL_DestroyWindow(s_window);
        s_window = nullptr;
    }
    SDL_Quit();
    s_keys.reset();
    s_mouse.reset();
}

bool platform_should_close() { return s_should_close; }

float platform_begin_frame()
{
    s_keys.begin_frame();
    s_mouse.begin_frame();
    s_mouse_wheel = 0.0f;

    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
        case SDL_QUIT:
            s_should_close = true;
            break;
        case SDL_KEYDOWN:
        case SDL_KEYUP: {
            const int key = sdl_to_platform_key(event.key.keysym.sym);
            if (key >= 0 && key < 256)
                s_keys.set(static_cast<std::size_t>(key), event.type == SDL_KEYDOWN, event.key.repeat != 0);
        } break;
        case SDL_TEXTINPUT:
            for (const char* p = event.text.text; *p; ++p) {
                const unsigned char value = (unsigned char)*p;
                if (value >= 128) continue;
                const int next = (s_char_tail + 1) % 64;
                if (next != s_char_head) {
                    s_char_queue[s_char_tail] = (char)value;
                    s_char_tail = next;
                }
            }
            break;
        case SDL_MOUSEMOTION:
            s_mouse_x = event.motion.x;
            s_mouse_y = event.motion.y;
            break;
        case SDL_MOUSEBUTTONDOWN:
        case SDL_MOUSEBUTTONUP: {
            int button = -1;
            if (event.button.button == SDL_BUTTON_LEFT) button = 0;
            else if (event.button.button == SDL_BUTTON_RIGHT) button = 1;
            else if (event.button.button == SDL_BUTTON_MIDDLE) button = 2;
            if (button >= 0) {
                s_mouse.set(static_cast<std::size_t>(button), event.type == SDL_MOUSEBUTTONDOWN);
                s_mouse_x = event.button.x;
                s_mouse_y = event.button.y;
            }
        } break;
        case SDL_MOUSEWHEEL:
            s_mouse_wheel += (float)event.wheel.y;
            break;
        case SDL_WINDOWEVENT:
            if (event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED ||
                event.window.event == SDL_WINDOWEVENT_RESIZED) {
                s_win_w = event.window.data1;
                s_win_h = event.window.data2;
                recompute_viewport();
            } else if (event.window.event == SDL_WINDOWEVENT_FOCUS_LOST) {
                // 키·마우스 held와 미전달 press를 취소한다. 문자 큐는 유지.
                s_keys.cancel();
                s_mouse.cancel();
            }
            break;
        }
    }

    const uint64_t now = SDL_GetPerformanceCounter();
    const float dt = (float)(now - s_frame_start) / (float)s_frequency;
    s_frame_start = now;
    return std::min(dt, 0.1f);
}

void platform_present()
{
    if (!s_window) return;
    SDL_GL_SwapWindow(s_window);
}

void* platform_gl_get_proc(const char* name)
{
    return SDL_GL_GetProcAddress(name);
}

void platform_viewport(int& x_out, int& y_out, int& w_out, int& h_out)
{
    // s_vp_* 는 창 좌상단 원점이다. GL 은 좌하단 원점이라 y 를 뒤집어 준다.
    // 중앙 정렬이어도 남는 높이가 홀수면 위아래 여백이 1픽셀 다르다.
    // 배치 정책과 관계없이 아래쪽 여백으로 변환한다.
    x_out = s_vp_x;
    y_out = s_win_h - s_vp_y - s_vp_h;
    w_out = s_vp_w;
    h_out = s_vp_h;
}

void platform_fatal_error(const char* message)
{
    if (!message || !*message) return;
    std::fprintf(stderr, "[fatal] %s\n", message);
    // 창이 없거나 이미 파괴됐을 수 있다 — nullptr 부모로 띄운다.
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Entris", message, nullptr);
}

void platform_end_frame()
{
    if (s_frequency == 0) return;
    // vsync 가 켜져 있으면 60Hz 목표, 꺼져 있으면 상한만 건다.
    // (win32.cpp 의 같은 함수와 같은 이유 — 무제한 렌더 루프 방지)
    constexpr double kUncappedMaxFps = 240.0;
    const double target = s_frame_pacing ? (1.0 / 60.0) : (1.0 / kUncappedMaxFps);
    const uint64_t now = SDL_GetPerformanceCounter();
    const double elapsed = (double)(now - s_frame_start) / (double)s_frequency;
    const double remaining = target - elapsed;
    if (remaining > 0.0)
        SDL_Delay((Uint32)std::max(0.0, remaining * 1000.0 - 0.5));
}

bool platform_key_pressed(int key)
{
    return key >= 0 && s_keys.pressed(static_cast<std::size_t>(key));
}

bool platform_key_down(int key)
{
    return key >= 0 && s_keys.down(static_cast<std::size_t>(key));
}

bool platform_input_cancelled() { return s_keys.cancelled(); }

char platform_get_char_pressed()
{
    if (s_char_head == s_char_tail) return 0;
    const char value = s_char_queue[s_char_head];
    s_char_head = (s_char_head + 1) % 64;
    return value;
}

int platform_mouse_x()
{
    return platform_detail::logical_mouse_axis(s_mouse_x, s_vp_x, s_vp_w, s_logical_w);
}

int platform_mouse_y()
{
    return platform_detail::logical_mouse_axis(s_mouse_y, s_vp_y, s_vp_h, s_logical_h);
}

bool platform_mouse_pressed(int button)
{
    return s_mouse.pressed(static_cast<std::size_t>(button));
}

bool platform_mouse_down(int button)
{
    return s_mouse.down(static_cast<std::size_t>(button));
}

bool platform_mouse_released(int button)
{
    return s_mouse.released(static_cast<std::size_t>(button));
}

float platform_mouse_wheel() { return s_mouse_wheel; }

double platform_get_time()
{
    return (double)(SDL_GetPerformanceCounter() - s_init_time) /
           (double)s_frequency;
}

void platform_set_window_size(int width, int height)
{
    if (!s_window || width <= 0 || height <= 0) return;
    if (s_fullscreen) {
        SDL_SetWindowFullscreen(s_window, 0);
        s_fullscreen = false;
    }
    SDL_SetWindowSize(s_window, width, height);
    SDL_SetWindowPosition(s_window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
    SDL_GetWindowSize(s_window, &s_win_w, &s_win_h);
    recompute_viewport();
}

void platform_display_size(int& w_out, int& h_out)
{
    w_out = h_out = 0;
    const int display = s_window ? SDL_GetWindowDisplayIndex(s_window) : 0;
    SDL_Rect bounds{};
    // usable bounds 는 작업 표시줄/독을 제외한 영역이다. 이걸 지원하지 않는
    // 플랫폼도 있어 실패하면 데스크톱 모드 전체 크기로 물러난다.
    if (display >= 0 && SDL_GetDisplayUsableBounds(display, &bounds) == 0 &&
        bounds.w > 0 && bounds.h > 0) {
        w_out = bounds.w;
        h_out = bounds.h;
        return;
    }
    SDL_DisplayMode mode{};
    if (SDL_GetDesktopDisplayMode(display < 0 ? 0 : display, &mode) == 0) {
        w_out = mode.w;
        h_out = mode.h;
    }
}

void platform_set_fullscreen(bool on)
{
    if (!s_window) return;
    if (SDL_SetWindowFullscreen(
            s_window, on ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0) != 0) {
        std::fprintf(stderr, "[SDL] fullscreen failed: %s\n", SDL_GetError());
        return;
    }
    s_fullscreen = on;
    SDL_GetWindowSize(s_window, &s_win_w, &s_win_h);
    recompute_viewport();
}

bool platform_fullscreen_supported() { return true; }
void platform_set_vsync(bool on)
{
    // GL swap interval을 요청한다. 소프트웨어 대기와 달리 버퍼 교체 동기화를
    // 제어하지만 드라이버가 요청을 지원/적용해야 한다. 호출 성공이나 실제 표시
    // 주기를 여기서는 검증하지 않으므로 tearing 제거를 무조건 보장하지 않는다.
    s_frame_pacing = on;
    if (s_glctx) SDL_GL_SetSwapInterval(on ? 1 : 0);
}
