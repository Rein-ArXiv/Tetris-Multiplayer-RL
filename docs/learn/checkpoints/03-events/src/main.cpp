#ifndef SDL_MAIN_HANDLED
#define SDL_MAIN_HANDLED
#endif
#include <SDL.h>

#include <cstdio>

int main(int, char**)
{
    SDL_SetMainReady();
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
        std::fprintf(stderr, "init failed: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow(
        "Tetris study: events",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        640, 480, 0);
    if (!window) {
        std::fprintf(stderr, "window failed: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    const Uint64 frequency = SDL_GetPerformanceFrequency();
    if (frequency == 0) {
        std::fputs("performance frequency is zero\n", stderr);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    Uint64 previous = SDL_GetPerformanceCounter();
    double report_elapsed = 0.0;
    unsigned frames = 0;
    unsigned events = 0;
    bool running = true;

    while (running) {
        const Uint64 frame_start = SDL_GetPerformanceCounter();
        const double dt = static_cast<double>(frame_start - previous)
                        / static_cast<double>(frequency);
        previous = frame_start;

        SDL_Event event{};
        while (SDL_PollEvent(&event) != 0) {
            ++events;
            if (event.type == SDL_QUIT) {
                running = false;
                break;
            }
        }
        if (!running) break; // No frame work after accepting a quit request.

        // Diagnostic work continues even when there are no input events.
        ++frames;
        report_elapsed += dt;
        if (report_elapsed >= 1.0) {
            std::printf("elapsed=%.3f frames=%u events=%u last_dt=%.6f\n",
                        report_elapsed, frames, events, dt);
            report_elapsed = 0.0;
            frames = 0;
            events = 0;
        }

        const Uint64 work_end = SDL_GetPerformanceCounter();
        const double work_seconds = static_cast<double>(work_end - frame_start)
                                  / static_cast<double>(frequency);
        const double remaining = (1.0 / 60.0) - work_seconds;
        if (remaining >= 0.001) {
            SDL_Delay(static_cast<Uint32>(remaining * 1000.0));
        }
    }

    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
