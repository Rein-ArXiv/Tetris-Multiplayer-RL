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
#include "renderer/raster_sources.h"
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
static bool checks(const GlApi& gl, const char* capture) {
    GLint major=0,minor=0,profile=0;
    gl.GetIntegerv(MajorVersion,&major);gl.GetIntegerv(MinorVersion,&minor);
    gl.GetIntegerv(ContextProfileMask,&profile);
    const auto* renderer=gl.GetString(Renderer);
    if(gl.GetError()||!renderer||!(major>3||(major==3&&minor>=3))||!(profile&CoreProfileBit))return false;
    std::printf("Actual GL %d.%d Core | %s\n",major,minor,reinterpret_cast<const char*>(renderer));
    int sample_buffers=-1;
    if(SDL_GL_GetAttribute(SDL_GL_MULTISAMPLEBUFFERS,&sample_buffers)!=0||sample_buffers!=0)return false;
    VertexBuffer buffer(gl);VertexArray vao(gl);
    if(!buffer.upload(study_mesh::make_triangle())||!vao.configure(buffer.name()))return false;
    int doublebuffer=0;
    if(SDL_GL_GetAttribute(SDL_GL_DOUBLEBUFFER,&doublebuffer)!=0)return false;
    gl.ReadBuffer(doublebuffer?Back:Front);
    std::array<study_raster::Point,3> positions{};
    const auto mesh=study_mesh::make_triangle();
    for(std::size_t i=0;i<mesh.size();++i){
        const auto n=study_coordinates::perspective_divide({mesh[i].x,mesh[i].y,0,1});
        if(!n)return false;
        const auto w=study_coordinates::to_window(*n,{0,0,width,height});
        if(!w)return false;
        positions[i]={w->x,w->y};
    }
    for(const auto& experiment:study_raster_shader::cases){
        Program program(gl);Unbind unbind{gl};
        {
            Shader vertex(gl),fragment(gl);
            if(!vertex.compile(VertexShader,experiment.vertex_source)||
               !fragment.compile(FragmentShader,experiment.fragment_source)||
               !program.link(vertex.name(),fragment.name()))return false;
        }
        if(!draw_triangle(gl,program.name(),vao.name(),width,height))return false;
        std::vector<unsigned char> image(width*height*4);
        gl.ReadPixels(0,0,width,height,RGBA,UnsignedByte,image.data());
        if(gl.GetError())return false;
        const std::string name=experiment.name;
        int inside=0,discarded=0,compared=0,boundaries=0;
        for(int y=0;y<height;++y)for(int x=0;x<width;++x){
            const auto sample=study_raster::sample_triangle(positions,study_raster::pixel_center(x,y));
            if(!sample)return false;
            if(sample->region==study_raster::Region::boundary){++boundaries;continue;}
            std::array<double,3> expected{{0.05,0.08,0.12}};
            if(sample->region==study_raster::Region::inside){
                ++inside;
                const bool hole=name=="cutout" && ((x/8+y/8)%2==0);
                if(hole)++discarded;
                else if(name=="flat")expected={0,0,1};
                else expected=study_raster::mix_rgb(sample->weights);
            }
            for(int component=0;component<3;++component){
                const int want=static_cast<int>(std::lround(expected[component]*255));
                const int got=image[(y*width+x)*4+component];
                if(std::abs(want-got)>3){
                    std::fprintf(stderr,"%s pixel(%d,%d) component%d expected%d got%d\n",
                                 experiment.name,x,y,component,want,got);return false;
                }
            }
            ++compared;
        }
        // This selected triangle has no exact pixel-center boundary hits.
        if(inside!=2048||boundaries!=0||compared!=width*height)return false;
        if(name=="cutout" && (discarded==0||discarded==inside))return false;
        if(capture){const auto path=std::string(capture)+"-"+name+".ppm";if(!save_ppm(gl,path.c_str()))return false;}
        std::printf("%s: %d pixels compared, %d covered, %d discarded, %d boundary points excluded\n",
                    experiment.name,compared,inside,discarded,boundaries);
    }
    std::puts("Pixel centers and affine weights match smooth RGB; flat is last-vertex blue; discard preserves clear background");
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
           SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS,0)!=0 ||
           SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES,0)!=0 ||
           SDL_GL_SetAttribute(SDL_GL_RED_SIZE,8)!=0 || SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE,8)!=0 ||
           SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE,8)!=0 || SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE,8)!=0)break;
        window=SDL_CreateWindow("raster readback",0,0,width,height,SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN);
        if(!window)break;
        context=SDL_GL_CreateContext(window);
        if(!context||SDL_GL_MakeCurrent(window,context)!=0)break;
        GlApi gl;if(!load(gl,SDL_GL_GetProcAddress))break;
        try{result=checks(gl,argc>1?argv[1]:nullptr)?0:1;}
        catch(const std::exception& e){std::fprintf(stderr,"Raster probe exception: %s\n",e.what());}
    }while(false);
    if(result)std::fprintf(stderr,"Raster probe failed; SDL diagnostic: %s\n",SDL_GetError());
    if(context)SDL_GL_DeleteContext(context);
    if(window)SDL_DestroyWindow(window);
    SDL_Quit();return result;
}
