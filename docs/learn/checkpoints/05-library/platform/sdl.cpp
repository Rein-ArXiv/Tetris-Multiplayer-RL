#include "platform/platform.h"
#include <SDL.h>
#include <cstdio>

namespace {
SDL_Window* window = nullptr;
bool runtime_ready = false;
bool should_close = true;
Uint64 frequency = 0;
Uint64 previous = 0;
Uint64 frame_start = 0;
}

bool platform_init(int width, int height, const char* title)
{
    if (runtime_ready) {
        std::fputs("already initialized\n", stderr);
        return false;
    }
    SDL_SetMainReady();
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
        std::fprintf(stderr, "init failed: %s\n", SDL_GetError());
        SDL_Quit();
        return false;
    }
    runtime_ready = true;
    window = SDL_CreateWindow(title, SDL_WINDOWPOS_CENTERED,
                              SDL_WINDOWPOS_CENTERED, width, height, 0);
    if (!window) {
        std::fprintf(stderr, "window failed: %s\n", SDL_GetError());
        platform_shutdown();
        return false;
    }
    frequency = SDL_GetPerformanceFrequency();
    if (frequency == 0) {
        std::fputs("performance frequency is zero\n", stderr);
        platform_shutdown();
        return false;
    }
    previous = frame_start = SDL_GetPerformanceCounter();
    should_close = false;
    return true;
}

void platform_shutdown() noexcept
{
    if (window) SDL_DestroyWindow(window);
    window = nullptr;
    if (runtime_ready) SDL_Quit();
    runtime_ready = false;
    should_close = true;
    frequency = previous = frame_start = 0;
}

bool platform_should_close() { return should_close; }

FrameInfo platform_begin_frame()
{
    if (!window) return {};
    frame_start = SDL_GetPerformanceCounter();
    FrameInfo frame{static_cast<double>(frame_start - previous)
                    / static_cast<double>(frequency), 0};
    previous = frame_start;
    SDL_Event event{};
    while (SDL_PollEvent(&event) != 0) {
        ++frame.events;
        if (event.type == SDL_QUIT) {
            should_close = true;
            break;
        }
    }
    return frame;
}

void platform_end_frame()
{
    if (!window || should_close) return;
    const double work_seconds = static_cast<double>(SDL_GetPerformanceCounter() - frame_start)
                              / static_cast<double>(frequency);
    const double remaining = (1.0 / 60.0) - work_seconds;
    if (remaining >= 0.001) SDL_Delay(static_cast<Uint32>(remaining * 1000.0));
}
