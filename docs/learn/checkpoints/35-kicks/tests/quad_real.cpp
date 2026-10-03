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
#include "renderer/quad_sources.h"
#include <string>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <vector>
using namespace study_gl;
constexpr int width=128,height=128;
static bool save_ppm(const GlApi& gl,const char* path) {
    if(!path)return true;
    std::vector<unsigned char> rgba(width*height*4);
    gl.ReadPixels(0,0,width,height,RGBA,UnsignedByte,rgba.data());
    if(gl.GetError())return false;
    FILE* file=std::fopen(path,"wb"); if(!file)return false;
    bool ok=std::fprintf(file,"P6\n%d %d\n255\n",width,height)>0;
    // GL rows start at the bottom; PPM stores the top row first.
    for(int y=height-1;y>=0;--y)for(int x=0;x<width;++x)
        if(std::fwrite(rgba.data()+(y*width+x)*4,1,3,file)!=3)ok=false;
    if(std::fclose(file)!=0)ok=false;
    return ok;
}
struct Unbind { const GlApi& gl;~Unbind(){gl.BindVertexArray(0);gl.UseProgram(0);} };
static std::vector<unsigned char> read_mask(const GlApi& gl) {
    std::vector<unsigned char> rgba(width*height*4),mask(width*height);
    gl.ReadPixels(0,0,width,height,RGBA,UnsignedByte,rgba.data());
    if(gl.GetError())return {};
    for(int i=0;i<width*height;++i){
        // Both diagnostic triangle colors are well above the dark clear green.
        mask[i]=rgba[i*4+1]>100;
    }
    return mask;
}
static bool checks(const GlApi& gl, const char* capture) {
    GLint major=0,minor=0,profile=0;
    gl.GetIntegerv(MajorVersion,&major);gl.GetIntegerv(MinorVersion,&minor);
    gl.GetIntegerv(ContextProfileMask,&profile);
    const auto* renderer=gl.GetString(Renderer);
    if(gl.GetError()||!renderer||!(major>3||(major==3&&minor>=3))||!(profile&CoreProfileBit))return false;
    std::printf("Actual GL %d.%d Core | %s\n",major,minor,reinterpret_cast<const char*>(renderer));
    int samples=-1,doublebuffer=0;
    if(SDL_GL_GetAttribute(SDL_GL_MULTISAMPLEBUFFERS,&samples)!=0||samples!=0||
       SDL_GL_GetAttribute(SDL_GL_DOUBLEBUFFER,&doublebuffer)!=0)return false;
    gl.ReadBuffer(doublebuffer?Back:Front);
    gl.FrontFace(CounterClockwise);gl.CullFace(Back);
    Program program(gl);
    {
        Shader vertex(gl),fragment(gl);
        if(!vertex.compile(VertexShader,study_quad_shader::vertex)||
           !fragment.compile(FragmentShader,study_quad_shader::fragment)||
           !program.link(vertex.name(),fragment.name()))return false;
    }
    for(const auto& experiment:study_quad_shader::cases){
        VertexBuffer buffer(gl);VertexArray vao(gl);Unbind unbind{gl};
        const auto vertices=study_quad::make_quad(experiment.reverse_first);
        if(!buffer.upload(vertices)||!vao.configure(buffer.name())||buffer.vertex_count()!=6)return false;
        GLint bytes=0;gl.GetBufferParameteriv(ArrayBuffer,BufferSize,&bytes);
        if(gl.GetError()||bytes!=48)return false;
        if(experiment.cull)gl.Enable(CullFaceCap);else gl.Disable(CullFaceCap);
        if(!draw_triangles(gl,program.name(),vao.name(),buffer.vertex_count(),0,6,width,height))return false;
        const auto full=read_mask(gl);if(full.empty())return false;
        if(capture){const auto file=std::string(capture)+"-"+experiment.name+".ppm";if(!save_ppm(gl,file.c_str()))return false;}
        // Observe each triangle independently. A full opaque image alone could
        // hide double coverage on a shared edge.
        gl.Disable(CullFaceCap);
        if(!draw_triangles(gl,program.name(),vao.name(),6,0,3,width,height))return false;
        const auto first=read_mask(gl);if(first.empty())return false;
        if(!draw_triangles(gl,program.name(),vao.name(),6,3,3,width,height))return false;
        const auto second=read_mask(gl);if(second.empty())return false;
        int count=0,shared_centers=0;
        for(int y=0;y<height;++y)for(int x=0;x<width;++x){
            const auto i=y*width+x;
            const bool expected=x>=32&&x<96&&y>=32&&y<96;
            if(int(first[i])+int(second[i])!=int(expected))return false;
            const bool culled_first=experiment.reverse_first&&experiment.cull;
            if(full[i]!=(culled_first?second[i]:static_cast<unsigned char>(expected)))return false;
            count+=full[i];
            if(expected&&x==y)++shared_centers;
        }
        std::printf("%s: full=%d, shared diagonal centers=%d; separate masks have no holes or overlap\n",
                    experiment.name,count,shared_centers);
    }
    gl.Disable(CullFaceCap);
    return gl.GetError()==0;
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
           SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER,0)!=0 ||
           SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS,0)!=0 ||
           SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES,0)!=0 ||
           SDL_GL_SetAttribute(SDL_GL_RED_SIZE,8)!=0 || SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE,8)!=0 ||
           SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE,8)!=0 || SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE,8)!=0)break;
        window=SDL_CreateWindow("quad readback",0,0,width,height,SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN);
        if(!window)break;
        context=SDL_GL_CreateContext(window);
        if(!context||SDL_GL_MakeCurrent(window,context)!=0)break;
        GlApi gl;if(!load(gl,SDL_GL_GetProcAddress))break;
        try{result=checks(gl,argc>1?argv[1]:nullptr)?0:1;}
        catch(const std::exception& e){std::fprintf(stderr,"Quad probe exception: %s\n",e.what());}
    }while(false);
    if(result)std::fprintf(stderr,"Quad probe failed; SDL diagnostic: %s\n",SDL_GetError());
    if(context)SDL_GL_DeleteContext(context);
    if(window)SDL_DestroyWindow(window);
    SDL_Quit();return result;
}
