#include <SDL.h>
#include <dlfcn.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// Linux-only test interposer. Never linked into teaching or game binaries.
int SDL_Init(Uint32 flags) {
    int (*real_init)(Uint32) = dlsym(RTLD_NEXT, "SDL_Init");
    fprintf(stderr, "probe:init\n");
    return real_init(flags);
}
void SDL_Quit(void) {
    void (*real_quit)(void) = dlsym(RTLD_NEXT, "SDL_Quit");
    fprintf(stderr, "probe:quit\n");
    real_quit();
}
void SDL_DestroyWindow(SDL_Window* window) {
    void (*real_destroy)(SDL_Window*) = dlsym(RTLD_NEXT, "SDL_DestroyWindow");
    fprintf(stderr, "probe:destroy\n");
    real_destroy(window);
}
Uint64 SDL_GetPerformanceFrequency(void) {
    if (strcmp(getenv("LEARN_SMOKE"), "frequency-fail") == 0) return 0;
    Uint64 (*real_frequency)(void) = dlsym(RTLD_NEXT, "SDL_GetPerformanceFrequency");
    return real_frequency();
}

SDL_Window* SDL_CreateWindow(const char* title, int x, int y, int w, int h, Uint32 flags) {
    if (strcmp(getenv("LEARN_SMOKE"), "window-fail") == 0) {
        SDL_SetError("injected window failure");
        return NULL;
    }
    SDL_Window* (*real_create)(const char*,int,int,int,int,Uint32) = dlsym(RTLD_NEXT, "SDL_CreateWindow");
    SDL_Window* window = real_create(title,x,y,w,h,flags);
    if (window && strcmp(getenv("LEARN_SMOKE"), "quit") == 0) {
        SDL_Event event = {0};
        event.type = SDL_QUIT;
        if (SDL_PushEvent(&event) != 1) abort();
    }
    return window;
}
int SDL_WaitEvent(SDL_Event* event) {
    if (strcmp(getenv("LEARN_SMOKE"), "wait-fail") == 0) {
        SDL_SetError("injected wait failure");
        return 0;
    }
    int (*real_wait)(SDL_Event*) = dlsym(RTLD_NEXT, "SDL_WaitEvent");
    return real_wait(event);
}
int SDL_PollEvent(SDL_Event* event) {
    static Uint64 started;
    int (*real_poll)(SDL_Event*) = dlsym(RTLD_NEXT, "SDL_PollEvent");
    if (!started) started = SDL_GetTicks64();
    if (strcmp(getenv("LEARN_SMOKE"), "timed-quit") == 0 && SDL_GetTicks64() - started >= 1400) {
        event->type = SDL_QUIT;
        return 1;
    }
    return real_poll(event);
}
