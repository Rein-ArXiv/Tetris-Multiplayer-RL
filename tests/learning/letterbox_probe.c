// SDL routing fixture, not a real high-DPI display.
#include "gl_probe.c"
void SDL_GL_GetDrawableSize(SDL_Window* target,int* w,int* h){
    if(target!=window)abort();
    *w=mode("zero-drawable")?0:2000;*h=800;
}
SDL_Window* SDL_GetMouseFocus(void){return mode("no-focus")?NULL:window;}
Uint32 SDL_GetMouseState(int* x,int* y){*x=mode("bar")?233:500;*y=200;return 0;}
Uint32 SDL_GetWindowFlags(SDL_Window* target){
    Uint32 (*real)(SDL_Window*)=dlsym(RTLD_NEXT,"SDL_GetWindowFlags");
    return real(target) | (mode("minimized")?SDL_WINDOW_MINIMIZED:0);
}
