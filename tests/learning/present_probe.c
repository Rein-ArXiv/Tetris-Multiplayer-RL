// Linux-only call-routing double. Real SDL dummy window, fabricated GL context.
#define SDL_GL_SetSwapInterval fixture_old_swap_interval
#include "gl_probe.c"
#undef SDL_GL_SetSwapInterval
static int swaps,requests,interval;
int study_swap_calls(void){return swaps;}
int study_interval_requests(void){return requests;}
int SDL_GL_SetSwapInterval(int value){
    ++requests;
    if(mode("swap-fail"))return SDL_SetError("injected interval failure");
    interval=value;return 0;
}
int SDL_GL_GetSwapInterval(void){return mode("report-unknown") ? 0 : interval;}
void SDL_GL_GetDrawableSize(SDL_Window* target,int* w,int* h){
    if(target!=window)abort();
    *w=mode("zero-drawable") ? 0 : 128;*h=128;
}
void SDL_GL_SwapWindow(SDL_Window* target){
    if(target!=window || !context || mode("current-fail") || mode("current-window-fail") || mode("zero-drawable"))abort();
    ++swaps;
}
