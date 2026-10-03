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
static study_blend::Rgba expected_pixel(const study_blend_scene::Case& c,
                                            int x,int y,double background_alpha) {
    // Independent numeric oracle, including intentionally wrong GL factors.
    study_blend::Rgba dst{0,0,0,background_alpha};
    for (int step=0;step<2;++step) {
        const int layer=c.reverse ? 1-step : step;
        const bool covered=y>=32&&y<96 && (layer ? x>=48&&x<112 : x>=16&&x<80);
        if(!covered)continue;
        const double scale=c.premultiplied ? 0.5 : 1.0;
        study_blend::Rgba src{layer?0:scale,0,layer?scale:0,0.5};
        if(c.disabled){dst=src;continue;}
        const double rgb_factor=c.premultiplied&&!c.wrong_factor ? 1.0 : 0.5;
        dst={src.r*rgb_factor+dst.r*0.5,0,src.b*rgb_factor+dst.b*0.5,
             (c.legacy_alpha?0.25:0.5)+dst.a*0.5};
    }
    return dst;
}
static bool checks(const GlApi& gl, const char* capture) {
    GLint major=0,minor=0,profile=0;
    gl.GetIntegerv(MajorVersion,&major);gl.GetIntegerv(MinorVersion,&minor);
    gl.GetIntegerv(ContextProfileMask,&profile);
    const auto* renderer=gl.GetString(Renderer);
    if(gl.GetError()||!renderer||!(major>3||(major==3&&minor>=3))||!(profile&CoreProfileBit))return false;
    std::printf("Actual GL %d.%d Core | %s\n",major,minor,reinterpret_cast<const char*>(renderer));
    int samples=-1,doublebuffer=0,alpha_bits=0;
    if(SDL_GL_GetAttribute(SDL_GL_MULTISAMPLEBUFFERS,&samples)!=0||samples!=0||
       SDL_GL_GetAttribute(SDL_GL_DOUBLEBUFFER,&doublebuffer)!=0||
       SDL_GL_GetAttribute(SDL_GL_ALPHA_SIZE,&alpha_bits)!=0||alpha_bits<8)return false;
    gl.ReadBuffer(doublebuffer?Back:Front);
    VertexBuffer buffer(gl);VertexArray vao(gl);Unbind unbind{gl};
    if(!buffer.upload(study_blend_scene::make_layers())||!vao.configure(buffer.name())||buffer.vertex_count()!=12)return false;
    GLint bytes=0;gl.GetBufferParameteriv(ArrayBuffer,BufferSize,&bytes);
    if(gl.GetError()||bytes!=96)return false;
    for(const auto& c:study_blend_scene::cases){
        Program program(gl);
        {
            Shader vertex(gl),fragment(gl);
            if(!vertex.compile(VertexShader,study_blend_scene::vertex)||
               !fragment.compile(FragmentShader,c.premultiplied?study_blend_scene::premul_fragment:study_blend_scene::straight_fragment)||
               !program.link(vertex.name(),fragment.name()))return false;
        }
        for(double background_alpha : {0.0,1.0}){
            if(!study_blend_scene::configure(gl,c)||
               !study_blend_scene::render(gl,program.name(),vao.name(),12,c,width,height,background_alpha))return false;
            std::vector<unsigned char> rgba(width*height*4);
            gl.ReadPixels(0,0,width,height,RGBA,UnsignedByte,rgba.data());
            if(gl.GetError())return false;
            for(int y=0;y<height;++y)for(int x=0;x<width;++x){
                const auto expected=expected_pixel(c,x,y,background_alpha);
                const double channels[]={expected.r,expected.g,expected.b,expected.a};
                for(int k=0;k<4;++k){
                    const int wanted=static_cast<int>(std::lround(channels[k]*255));
                    const int actual=rgba[(y*width+x)*4+k];
                    if(std::abs(actual-wanted)>2){
                        std::fprintf(stderr,"%s alpha %.0f pixel %d,%d channel %d expected %d got %d\n",c.name,background_alpha,x,y,k,wanted,actual);return false;
                    }
                }
            }
            const int i=(64*width+64)*4;
            std::printf("%s background alpha %.0f: center RGBA %u %u %u %u; all 16384 pixels passed\n",
                c.name,background_alpha,rgba[i],rgba[i+1],rgba[i+2],rgba[i+3]);
            if(capture && background_alpha==1){
                const auto file=std::string(capture)+"-"+c.name+".ppm";
                if(!save_ppm(gl,file.c_str()))return false;
            }
        }
    }
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
        window=SDL_CreateWindow("blend readback",0,0,width,height,SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN);
        if(!window)break;
        context=SDL_GL_CreateContext(window);
        if(!context||SDL_GL_MakeCurrent(window,context)!=0)break;
        GlApi gl;if(!load(gl,SDL_GL_GetProcAddress))break;
        try{result=checks(gl,argc>1?argv[1]:nullptr)?0:1;}
        catch(const std::exception& e){std::fprintf(stderr,"Blend probe exception: %s\n",e.what());}
    }while(false);
    if(result)std::fprintf(stderr,"Blend probe failed; SDL diagnostic: %s\n",SDL_GetError());
    if(context)SDL_GL_DeleteContext(context);
    if(window)SDL_DestroyWindow(window);
    SDL_Quit();return result;
}
