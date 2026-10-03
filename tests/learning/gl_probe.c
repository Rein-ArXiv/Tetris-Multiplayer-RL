// Linux test double for failure ordering. No GL rendering is validated here.
#include <SDL.h>
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static SDL_Window* window;
static void* context;
static int mode(const char* name) {
    const char* value = getenv("LEARN_GL");
    return value && strcmp(value, name) == 0;
}
int SDL_Init(Uint32 flags) {
    fprintf(stderr, "glprobe:init\n");
    if (mode("init-fail")) return SDL_SetError("injected init failure");
    int (*real)(Uint32) = dlsym(RTLD_NEXT,"SDL_Init");
    return real(flags);
}
int SDL_GL_SetAttribute(SDL_GLattr attr, int value) {
    (void)attr; (void)value;
    if (mode("attribute-fail")) return SDL_SetError("injected attribute failure");
    return 0;
}
SDL_Window* SDL_CreateWindow(const char* title,int x,int y,int w,int h,Uint32 flags) {
    if (mode("window-fail")) { SDL_SetError("injected window failure"); return NULL; }
    SDL_Window* (*real)(const char*,int,int,int,int,Uint32) = dlsym(RTLD_NEXT,"SDL_CreateWindow");
    // Only the fixture strips OPENGL. The actual binary requests a GL window.
    window = real(title,x,y,w,h,flags & ~SDL_WINDOW_OPENGL);
    return window;
}
SDL_GLContext SDL_GL_CreateContext(SDL_Window* target) {
    if (target != window || context) abort();
    if (mode("context-fail")) { SDL_SetError("injected context failure"); return NULL; }
    context = malloc(1);
    if (!context) abort();
    return context;
}
SDL_GLContext SDL_GL_GetCurrentContext(void) { return mode("current-fail") ? NULL : context; }
SDL_Window* SDL_GL_GetCurrentWindow(void) { return mode("current-window-fail") ? NULL : window; }
int SDL_GL_MakeCurrent(SDL_Window* target,SDL_GLContext ctx) {
    if (target != window || ctx != context) abort();
    return mode("makecurrent-fail") ? SDL_SetError("injected makecurrent failure") : 0;
}
int SDL_GL_SetSwapInterval(int interval) {
    (void)interval;
    return mode("swap-fail") ? SDL_SetError("injected swap failure") : 0;
}
int SDL_GL_GetAttribute(SDL_GLattr attr,int* value) {
    if (mode("query-fail")) return SDL_SetError("injected query failure");
    switch(attr) {
    case SDL_GL_CONTEXT_MAJOR_VERSION: *value = mode("higher") ? 4 : 3; break;
    case SDL_GL_CONTEXT_MINOR_VERSION: *value = mode("higher") ? 0 : mode("version-fail") ? 2 : 3; break;
    case SDL_GL_CONTEXT_PROFILE_MASK: *value = mode("profile-fail") ? SDL_GL_CONTEXT_PROFILE_COMPATIBILITY : SDL_GL_CONTEXT_PROFILE_CORE; break;
    case SDL_GL_DOUBLEBUFFER: *value = mode("buffer-fail") ? 0 : 1; break;
    default: abort();
    }
    return 0;
}
Uint64 SDL_GetPerformanceFrequency(void) {
    if (mode("frequency-fail")) return 0;
    Uint64 (*real)(void) = dlsym(RTLD_NEXT,"SDL_GetPerformanceFrequency");
    return real();
}
void SDL_GL_DeleteContext(SDL_GLContext ctx) {
    if (ctx != context || !window) abort();
    fprintf(stderr,"glprobe:context\n"); free(context); context = NULL;
}
void SDL_DestroyWindow(SDL_Window* target) {
    if (context || target != window) abort();
    fprintf(stderr,"glprobe:window\n");
    void (*real)(SDL_Window*) = dlsym(RTLD_NEXT,"SDL_DestroyWindow");
    real(target); window = NULL;
}
void SDL_Quit(void) {
    if (context || window) abort();
    fprintf(stderr,"glprobe:quit\n");
    void (*real)(void) = dlsym(RTLD_NEXT,"SDL_Quit"); real();
}
