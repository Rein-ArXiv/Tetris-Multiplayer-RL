// Explicit platform-call double: never evidence of actual presentation.
#include "gl_probe.c"
void SDL_GL_GetDrawableSize(SDL_Window* target, int* w, int* h) {
    if(target!=window || !context) abort();
    *w=mode("zero-drawable")?0:1280;
    *h=mode("zero-drawable")?0:960;
}
Uint32 SDL_GetWindowFlags(SDL_Window* target) {
    if(target!=window) abort();
    return mode("minimized") ? SDL_WINDOW_MINIMIZED : 0;
}
void SDL_GL_SwapWindow(SDL_Window* target) {
    if(target!=window || !context || mode("zero-drawable") || mode("minimized")) abort();
    fprintf(stderr,"drawprobe:swap\n");
}
