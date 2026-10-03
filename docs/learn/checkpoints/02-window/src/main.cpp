#ifndef SDL_MAIN_HANDLED
#define SDL_MAIN_HANDLED
#endif
#include <SDL.h>

#include <cstdio>

int main(int, char**)
{
    SDL_SetMainReady();
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        std::fprintf(stderr, "init failed: %s\n", SDL_GetError());
        SDL_Quit(); // Safe even when initialization fails.
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow(
        "Tetris study: window",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        640, 480, 0); // Visible by default; no GL context yet.
    if (!window) {
        std::fprintf(stderr, "window failed: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    int exit_code = 0;
    bool running = true;
    while (running) {
        SDL_Event event{};
        if (SDL_WaitEvent(&event) == 0) {
            std::fprintf(stderr, "wait failed: %s\n", SDL_GetError());
            exit_code = 1;
            break; // Do not interpret event after a failed call.
        }
        if (event.type == SDL_QUIT) {
            running = false;
        }
    }

    SDL_DestroyWindow(window);
    SDL_Quit();
    return exit_code;
}
