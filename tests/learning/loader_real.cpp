// Independent, non-presenting SDL probe. It does not relax the window demo's
// double-buffer policy; an offscreen query has no swap requirement.
#ifndef SDL_MAIN_HANDLED
#define SDL_MAIN_HANDLED
#endif
#include <SDL.h>
#include "renderer/gl_api.h"
#include <cstdio>
int main() {
    SDL_SetMainReady();
    if (SDL_Init(SDL_INIT_VIDEO) != 0) return 1;
    SDL_Window* window = nullptr;
    SDL_GLContext context = nullptr;
    int result = 1;
    do {
        if (SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,3) != 0 ||
            SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,3) != 0 ||
            SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_CORE) != 0 ||
            SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS,SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG)!=0) break;
        window = SDL_CreateWindow("loader probe",0,0,64,64,SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN);
        if (!window) break;
        context = SDL_GL_CreateContext(window);
        if (!context) break;
        int major=0,minor=0,profile=0;
        if (SDL_GL_GetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,&major) != 0 ||
            SDL_GL_GetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,&minor) != 0 ||
            SDL_GL_GetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,&profile) != 0 ||
            !(major>3 || (major==3 && minor>=3)) || profile != SDL_GL_CONTEXT_PROFILE_CORE) break;
        study_gl::GlApi gl;
        if (!study_gl::load(gl,SDL_GL_GetProcAddress)) break;
        const auto* version=gl.GetString(study_gl::Version);
        const auto* renderer=gl.GetString(study_gl::Renderer);
        study_gl::GLint attributes=0, actual_major=0, actual_minor=0, actual_profile=0;
        gl.GetIntegerv(study_gl::MajorVersion,&actual_major);
        gl.GetIntegerv(study_gl::MinorVersion,&actual_minor);
        gl.GetIntegerv(study_gl::ContextProfileMask,&actual_profile);
        gl.GetIntegerv(study_gl::MaxVertexAttribs,&attributes);
        if (!version || !renderer || attributes<=0 || gl.GetError()!=0 ||
            !(actual_major>3 || (actual_major==3 && actual_minor>=3)) ||
            !(actual_profile & study_gl::CoreProfileBit)) break;
        std::printf("Real GL query: %s | %s | actual=%d.%d core, max attributes=%d\n",
                    reinterpret_cast<const char*>(version),reinterpret_cast<const char*>(renderer),actual_major,actual_minor,attributes);
        result=0;
    } while(false);
    if (result) std::fprintf(stderr,"real loader probe failed: %s\n",SDL_GetError());
    if (context) SDL_GL_DeleteContext(context);
    if (window) SDL_DestroyWindow(window);
    SDL_Quit();
    return result;
}
