// platform/sdl.cpp — SDL2 window/input/timer + surface framebuffer presentation

#include <SDL2/SDL.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

#include "platform.h"

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
    s_window = SDL_CreateWindow(
        title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        width, height, SDL_WINDOW_SHOWN);
    if (!s_window) {
        std::fprintf(stderr, "[SDL] window creation failed: %s\n", SDL_GetError());
        s_should_close = true;
        return;
    }
    SDL_StartTextInput();
    s_frequency = SDL_GetPerformanceFrequency();
    s_init_time = SDL_GetPerformanceCounter();
    s_frame_start = s_init_time;
}

void platform_shutdown()
{
    SDL_StopTextInput();
    if (s_window) {
        SDL_DestroyWindow(s_window);
        s_window = nullptr;
    }
    SDL_Quit();
}

bool platform_should_close() { return s_should_close; }

float platform_begin_frame()
{
    std::memcpy(s_key_prev, s_key_state, sizeof(s_key_state));
    std::memcpy(s_mouse_prev, s_mouse_state, sizeof(s_mouse_state));
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
                s_key_state[key] = event.type == SDL_KEYDOWN;
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
                s_mouse_state[button] = event.type == SDL_MOUSEBUTTONDOWN;
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
            }
            break;
        }
    }

    const uint64_t now = SDL_GetPerformanceCounter();
    const float dt = (float)(now - s_frame_start) / (float)s_frequency;
    s_frame_start = now;
    return std::min(dt, 0.1f);
}

void platform_present(const uint32_t* pixels, int width, int height,
                      int pitch_bytes)
{
    if (!s_window || !pixels || width <= 0 || height <= 0) return;
    SDL_Surface* window_surface = SDL_GetWindowSurface(s_window);
    if (!window_surface) {
        std::fprintf(stderr, "[SDL] window surface failed: %s\n", SDL_GetError());
        return;
    }
    s_win_w = window_surface->w;
    s_win_h = window_surface->h;
    recompute_viewport();

    SDL_Surface* frame = SDL_CreateRGBSurfaceFrom(
        const_cast<uint32_t*>(pixels), width, height, 32, pitch_bytes,
        0x00FF0000u, 0x0000FF00u, 0x000000FFu, 0xFF000000u);
    if (!frame) {
        std::fprintf(stderr, "[SDL] framebuffer wrapper failed: %s\n",
                     SDL_GetError());
        return;
    }
    SDL_FillRect(window_surface, nullptr,
                 SDL_MapRGB(window_surface->format, 0, 0, 0));
    SDL_Rect destination{s_vp_x, s_vp_y, s_vp_w, s_vp_h};
    if (SDL_BlitScaled(frame, nullptr, window_surface, &destination) != 0)
        std::fprintf(stderr, "[SDL] framebuffer blit failed: %s\n", SDL_GetError());
    SDL_FreeSurface(frame);
    SDL_UpdateWindowSurface(s_window);
}

void platform_end_frame()
{
    if (!s_frame_pacing || s_frequency == 0) return;
    const double target = 1.0 / 60.0;
    const uint64_t now = SDL_GetPerformanceCounter();
    const double elapsed = (double)(now - s_frame_start) / (double)s_frequency;
    const double remaining = target - elapsed;
    if (remaining > 0.0)
        SDL_Delay((Uint32)std::max(0.0, remaining * 1000.0 - 0.5));
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
void platform_set_vsync(bool on) { s_frame_pacing = on; }
