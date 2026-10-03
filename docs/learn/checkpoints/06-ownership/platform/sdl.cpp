#include "platform/platform.h"
#include <SDL.h>
#include <cstdio>
#include <exception>
#include <memory>
#include <utility>

namespace {
// This teaching executable is the sole owner of SDL's global lifetime.
class SdlRuntime {
public:
    SdlRuntime() {
        SDL_SetMainReady();
        ready_ = SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0;
    }
    ~SdlRuntime() noexcept { SDL_Quit(); }
    SdlRuntime(const SdlRuntime&) = delete;
    SdlRuntime& operator=(const SdlRuntime&) = delete;
    bool ready() const noexcept { return ready_; }
private:
    bool ready_;
};

struct WindowDeleter {
    void operator()(SDL_Window* window) const noexcept { SDL_DestroyWindow(window); }
};
using WindowOwner = std::unique_ptr<SDL_Window, WindowDeleter>;

struct Resources {
    // Members are destroyed in reverse declaration order: window before runtime.
    SdlRuntime runtime;
    WindowOwner window;
    Uint64 frequency = 0;
    Uint64 previous = 0;
    Uint64 frame_start = 0;
    bool should_close = false;
};
std::unique_ptr<Resources> active;
}

bool platform_init(int width, int height, const char* title)
{
    if (active) {
        std::fputs("already initialized\n", stderr);
        return false;
    }
    try {
        auto candidate = std::make_unique<Resources>();
        if (!candidate->runtime.ready()) {
            std::fprintf(stderr, "init failed: %s\n", SDL_GetError());
            return false;
        }
        candidate->window.reset(SDL_CreateWindow(title, SDL_WINDOWPOS_CENTERED,
                                SDL_WINDOWPOS_CENTERED, width, height, 0));
        if (!candidate->window) {
            std::fprintf(stderr, "window failed: %s\n", SDL_GetError());
            return false;
        }
        candidate->frequency = SDL_GetPerformanceFrequency();
        if (candidate->frequency == 0) {
            std::fputs("performance frequency is zero\n", stderr);
            return false;
        }
        candidate->previous = candidate->frame_start = SDL_GetPerformanceCounter();
        active = std::move(candidate); // Publish only a fully initialized owner.
        return true;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "initialization exception: %s\n", error.what());
        return false;
    }
}

void platform_shutdown() noexcept { active.reset(); }
bool platform_should_close() { return !active || active->should_close; }

FrameInfo platform_begin_frame()
{
    if (!active) return {};
    auto& state = *active;
    state.frame_start = SDL_GetPerformanceCounter();
    FrameInfo frame{static_cast<double>(state.frame_start - state.previous)
                    / static_cast<double>(state.frequency), 0};
    state.previous = state.frame_start;
    SDL_Event event{};
    while (SDL_PollEvent(&event) != 0) {
        ++frame.events;
        if (event.type == SDL_QUIT) {
            state.should_close = true;
            break;
        }
    }
    return frame;
}

void platform_end_frame()
{
    if (!active || active->should_close) return;
    const auto& state = *active;
    const double work_seconds = static_cast<double>(SDL_GetPerformanceCounter() - state.frame_start)
                              / static_cast<double>(state.frequency);
    const double remaining = (1.0 / 60.0) - work_seconds;
    if (remaining >= 0.001) SDL_Delay(static_cast<Uint32>(remaining * 1000.0));
}
