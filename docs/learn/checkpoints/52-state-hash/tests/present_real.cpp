// Pixel readback observes rendering, not monitor presentation or frame timing.
#ifndef SDL_MAIN_HANDLED
#define SDL_MAIN_HANDLED
#endif
#include <SDL.h>
#include "renderer/triangle.h"
#include "renderer/vertex_array.h"
#include "renderer/vertex_buffer.h"
#include "renderer/program.h"
#include "renderer/shader.h"
#include "renderer/shader_sources.h"
#include "renderer/coordinates.h"
#include "renderer/raster.h"
#include "renderer/quad.h"
#include "renderer/blend_scene.h"
#include <cmath>
#include <chrono>
#include "renderer/submission.h"
#include "renderer/cpu_timing.h"
#include <string>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <vector>
using namespace study_gl;
constexpr int width=128,height=128;
struct Unbind { const GlApi& gl;~Unbind(){gl.BindVertexArray(0);gl.UseProgram(0);} };
static bool checks(const GlApi& gl, const char*) {
    GLint major=0,minor=0,profile=0;
    gl.GetIntegerv(MajorVersion,&major);gl.GetIntegerv(MinorVersion,&minor);
    gl.GetIntegerv(ContextProfileMask,&profile);
    const auto* renderer=gl.GetString(Renderer);
    if(gl.GetError()||!renderer||!(major>3||(major==3&&minor>=3))||!(profile&CoreProfileBit))return false;
    std::printf("Actual GL %d.%d Core | %s\n",major,minor,reinterpret_cast<const char*>(renderer));
    int doublebuffer=0,samples=-1;
    if(SDL_GL_GetAttribute(SDL_GL_DOUBLEBUFFER,&doublebuffer)!=0||
       SDL_GL_GetAttribute(SDL_GL_MULTISAMPLEBUFFERS,&samples)!=0||samples!=0)return false;
    std::printf("Requested double buffer=1; reported=%d. Display completion is not observed.\n",doublebuffer);
    for(int requested : {0,1}){
        const int result=SDL_GL_SetSwapInterval(requested);
        std::printf("interval request=%d accepted=%d reported=%d (0 may also mean unknown)\n",
                    requested,result==0,SDL_GL_GetSwapInterval());
    }
    gl.ReadBuffer(doublebuffer?Back:Front);
    VertexBuffer buffer(gl);VertexArray vao(gl);Program program(gl);Unbind unbind{gl};
    if(!buffer.upload(study_blend_scene::make_layers())||!vao.configure(buffer.name()))return false;
    {
        Shader vertex(gl),fragment(gl);
        if(!vertex.compile(VertexShader,study_blend_scene::vertex)||
           !fragment.compile(FragmentShader,study_blend_scene::straight_fragment)||
           !program.link(vertex.name(),fragment.name()))return false;
    }
    const auto& c=study_blend_scene::cases[0];
    for(auto mode : {study_submission::Mode::submit,study_submission::Mode::flush,study_submission::Mode::finish}){
        if(!study_blend_scene::configure(gl,c))return false;
        const auto origin=std::chrono::steady_clock::now();
        const auto ticks=[&](){return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now()-origin).count());};
        if(!study_blend_scene::render(gl,program.name(),vao.name(),12,c,width,height))return false;
        const auto submitted=ticks();
        if(!study_submission::boundary(gl,mode))return false;
        const auto synchronized=ticks();
        const auto timing=study_timing::summarize(0,submitted,synchronized,synchronized,1000000000);
        if(!timing)return false;
        // Readback is deliberately OUTSIDE the timing region. It can itself
        // wait for rendering. An image read after submit is not proof that
        // rendering had already finished when the earlier draw call returned.
        std::vector<unsigned char> rgba(width*height*4);
        gl.ReadPixels(0,0,width,height,RGBA,UnsignedByte,rgba.data());
        if(gl.GetError())return false;
        for(int y=0;y<height;++y)for(int x=0;x<width;++x){
            const bool a=y>=32&&y<96&&x>=16&&x<80;
            const bool b=y>=32&&y<96&&x>=48&&x<112;
            const int expected[]={a?(b?64:128):0,0,b?128:0};
            for(int k=0;k<3;++k)if(std::abs(int(rgba[(y*width+x)*4+k])-expected[k])>2)return false;
        }
        std::printf("mode=%d CPU submit=%.3fms sync=%.3fms; RGB readback verified later\n",
                    static_cast<int>(mode),timing->submit_ms,timing->sync_ms);
        // This path is conditional; single-buffer environments do not pass
        // a windowed presentation test merely by passing the RGB checks.
        if(doublebuffer)SDL_GL_SwapWindow(SDL_GL_GetCurrentWindow());
    }
    return true;
}
int main(int argc,char** argv) {
    SDL_SetMainReady();
    if(SDL_Init(SDL_INIT_VIDEO)!=0){std::fprintf(stderr,"SDL init: %s\n",SDL_GetError());return 1;}
    SDL_Window* window=nullptr;SDL_GLContext context=nullptr;int result=1;
    do {
        if(SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,3)!=0 ||
           SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,3)!=0 ||
           SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_CORE)!=0 ||
            SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS,SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG)!=0 ||
           SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER,1)!=0 ||
           SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS,0)!=0 ||
           SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES,0)!=0 ||
           SDL_GL_SetAttribute(SDL_GL_RED_SIZE,8)!=0 || SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE,8)!=0 ||
           SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE,8)!=0 || SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE,8)!=0)break;
        window=SDL_CreateWindow("submission readback",0,0,width,height,SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN);
        if(!window)break;
        context=SDL_GL_CreateContext(window);
        if(!context||SDL_GL_MakeCurrent(window,context)!=0)break;
        GlApi gl;if(!load(gl,SDL_GL_GetProcAddress))break;
        try{result=checks(gl,argc>1?argv[1]:nullptr)?0:1;}
        catch(const std::exception& e){std::fprintf(stderr,"Submission probe exception: %s\n",e.what());}
    }while(false);
    if(result)std::fprintf(stderr,"Submission probe failed; SDL diagnostic: %s\n",SDL_GetError());
    if(context)SDL_GL_DeleteContext(context);
    if(window)SDL_DestroyWindow(window);
    SDL_Quit();return result;
}
