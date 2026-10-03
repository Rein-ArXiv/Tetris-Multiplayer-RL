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
#include <array>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <vector>
using namespace study_gl;
constexpr int width=128,height=128;
static bool pixel(const GlApi& gl,int x,int y,int r,int g,int b) {
    std::array<unsigned char,4> rgba{};
    // RGBA byte rows are multiples of four, matching default PACK_ALIGNMENT.
    // No pixel-pack buffer is bound: the last argument is a CPU address.
    gl.ReadPixels(x,y,1,1,RGBA,UnsignedByte,rgba.data());
    if(gl.GetError()) return false;
    const bool ok=std::abs(int(rgba[0])-r)<=3 && std::abs(int(rgba[1])-g)<=3 && std::abs(int(rgba[2])-b)<=3;
    if(!ok) std::fprintf(stderr,"pixel(%d,%d) expected %d,%d,%d got %u,%u,%u\n",x,y,r,g,b,rgba[0],rgba[1],rgba[2]);
    return ok;
}
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
static bool checks(const GlApi& gl, const char* capture) {
    GLint major=0,minor=0,profile=0;
    gl.GetIntegerv(MajorVersion,&major);gl.GetIntegerv(MinorVersion,&minor);gl.GetIntegerv(ContextProfileMask,&profile);
    const auto* renderer=gl.GetString(Renderer);
    if(gl.GetError()||!renderer||!(major>3||(major==3&&minor>=3))||!(profile&CoreProfileBit))return false;
    std::printf("Actual GL %d.%d Core | %s\n",major,minor,reinterpret_cast<const char*>(renderer));
    VertexBuffer buffer(gl);VertexArray vao(gl);Program program(gl);
    Unbind unbind{gl};
    if(!buffer.upload(study_mesh::make_triangle())||!vao.configure(buffer.name()))return false;
    {
        Shader vertex(gl),fragment(gl);
        if(!vertex.compile(VertexShader,study_shader::vertex)||!fragment.compile(FragmentShader,study_shader::fragment)||
           !program.link(vertex.name(),fragment.name()))return false;
    }
    int doublebuffer=0;
    if(SDL_GL_GetAttribute(SDL_GL_DOUBLEBUFFER,&doublebuffer)!=0)return false;
    gl.ReadBuffer(doublebuffer?Back:Front);
    // Generic ARRAY_BUFFER binding need not remain selected for the VAO to read it.
    gl.BindBuffer(ArrayBuffer,0);
    if(gl.GetError()||!draw_triangle(gl,program.name(),vao.name(),width,height))return false;
    if(!pixel(gl,64,60,51,179,230)||!pixel(gl,4,4,13,20,31)||!save_ppm(gl,capture))return false;
    GLint current=-1,array=-1;
    gl.GetIntegerv(CurrentProgram,&current);gl.GetIntegerv(VertexArrayBinding,&array);
    if(gl.GetError()||current!=0||array!=0)return false;
    std::puts("Triangle interior and background pixels match; ARRAY_BUFFER zero still draws via captured VAO");

    gl.UseProgram(program.name());gl.BindVertexArray(vao.name());
    gl.Clear(ColorBufferBit);gl.DrawArrays(Triangles,0,2);
    if(gl.GetError()||!pixel(gl,64,60,13,20,31))return false;
    std::puts("Two vertices: no complete triangle, no GL API error, background remains");

    gl.DrawArrays(Triangles,0,3);
    if(gl.GetError()||!pixel(gl,64,60,51,179,230))return false;
    gl.Clear(ColorBufferBit);
    if(gl.GetError()||!pixel(gl,64,60,13,20,31))return false;
    std::puts("Clear after draw erases the triangle");

    gl.Viewport(0,0,0,0);gl.ClearColor(0.4f,0.1f,0.2f,1.0f);gl.Clear(ColorBufferBit);
    gl.DrawArrays(Triangles,0,3);
    if(gl.GetError()||!pixel(gl,64,60,102,26,51)||!pixel(gl,4,4,102,26,51))return false;
    std::puts("Zero viewport: clear still covers attachment, draw produces no triangle pixels");
    gl.BindVertexArray(0);gl.UseProgram(0);
    if(gl.GetError()||!draw_triangle(gl,program.name(),vao.name(),width,height)||!pixel(gl,64,60,51,179,230))return false;
    GLint viewport[4]{};gl.GetIntegerv(ViewportState,viewport);
    if(gl.GetError()||viewport[0]!=0||viewport[1]!=0||viewport[2]!=width||viewport[3]!=height)return false;
    std::puts("Next pass resets viewport and redraws; CPU pixel readback completed without a separate glFinish");
    return true;
}
int main(int argc,char** argv) {
    SDL_SetMainReady(); // Explicit console entry (SDL_MAIN_HANDLED).
    if(SDL_Init(SDL_INIT_VIDEO)!=0){std::fprintf(stderr,"SDL init: %s\n",SDL_GetError());return 1;}
    SDL_Window* window=nullptr;SDL_GLContext context=nullptr;int result=1;
    do {
        if(SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,3)!=0 ||
           SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,3)!=0 ||
           SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_CORE)!=0 ||
            SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS,SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG)!=0 ||
           SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER,0)!=0 ||
           SDL_GL_SetAttribute(SDL_GL_RED_SIZE,8)!=0 || SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE,8)!=0 ||
           SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE,8)!=0 || SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE,8)!=0)break;
        window=SDL_CreateWindow("triangle readback",0,0,width,height,SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN);
        if(!window)break;
        context=SDL_GL_CreateContext(window);
        if(!context||SDL_GL_MakeCurrent(window,context)!=0)break;
        GlApi gl;if(!load(gl,SDL_GL_GetProcAddress))break;
        try{result=checks(gl,argc>1?argv[1]:nullptr)?0:1;}
        catch(const std::exception& e){std::fprintf(stderr,"Triangle probe exception: %s\n",e.what());}
    }while(false);
    if(result)std::fprintf(stderr,"Triangle probe failed; SDL diagnostic: %s\n",SDL_GetError());
    if(context)SDL_GL_DeleteContext(context);
    if(window)SDL_DestroyWindow(window);
    SDL_Quit();return result;
}
