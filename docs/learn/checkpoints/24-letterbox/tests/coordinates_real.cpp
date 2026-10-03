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
#include "renderer/projection_cases.h"
#include <string>
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
    gl.GetIntegerv(MajorVersion,&major);gl.GetIntegerv(MinorVersion,&minor);
    gl.GetIntegerv(ContextProfileMask,&profile);
    const auto* renderer=gl.GetString(Renderer);
    if(gl.GetError()||!renderer||!(major>3||(major==3&&minor>=3))||!(profile&CoreProfileBit))return false;
    std::printf("Actual GL %d.%d Core | %s\n",major,minor,reinterpret_cast<const char*>(renderer));
    VertexBuffer buffer(gl);VertexArray vao(gl);
    if(!buffer.upload(study_mesh::make_triangle())||!vao.configure(buffer.name()))return false;
    int doublebuffer=0;
    if(SDL_GL_GetAttribute(SDL_GL_DOUBLEBUFFER,&doublebuffer)!=0)return false;
    gl.ReadBuffer(doublebuffer?Back:Front);
    std::vector<unsigned char> base_image;
    int base_count=0;
    for(const auto& experiment:study_projection::cases){
        Program program(gl);Unbind unbind{gl};
        {
            Shader vertex(gl),fragment(gl);
            if(!vertex.compile(VertexShader,experiment.vertex_source)||
               !fragment.compile(FragmentShader,study_shader::fragment)||
               !program.link(vertex.name(),fragment.name()))return false;
        }
        if(!draw_triangle(gl,program.name(),vao.name(),width,height))return false;
        if(!pixel(gl,64,60,51,179,230)||!pixel(gl,124,124,13,20,31))return false;
        const std::string name=experiment.name;
        if(name=="w2"){
            if(!pixel(gl,44,44,13,20,31))return false;
        }else if(!pixel(gl,44,44,51,179,230))return false;
        std::vector<unsigned char> image(width*height*4);
        gl.ReadPixels(0,0,width,height,RGBA,UnsignedByte,image.data());
        if(gl.GetError())return false;
        int count=0,min_x=width,max_x=-1,min_y=height,max_y=-1;
        for(int y=0;y<height;++y)for(int x=0;x<width;++x){
            const auto* rgb=image.data()+(y*width+x)*4;
            if(rgb[1]>100){
                ++count; if(x<min_x)min_x=x;if(x>max_x)max_x=x;
                if(y<min_y)min_y=y;
                if(y>max_y)max_y=y;
            }
        }
        // Compare observed pixel bounds to CPU continuous vertex bounds, away
        // from clipping. One pixel of tolerance covers edge/sample convention.
        if(name!="oversized"){
            double left=width,right=0,bottom=height,top=0;
            for(const auto& v:study_mesh::make_triangle()){
                const auto n=study_coordinates::perspective_divide(study_projection::clip_for(experiment,v.x,v.y));
                if(!n)return false;
                const auto w=study_coordinates::to_window(*n,{0,0,width,height});
                if(!w)return false;
                if(w->x<left)left=w->x;
                if(w->x>right)right=w->x;
                if(w->y<bottom)bottom=w->y;
                if(w->y>top)top=w->y;
            }
            if(count==0||std::abs(min_x-left)>1||std::abs(max_x+1-right)>1||
               std::abs(min_y-bottom)>1||std::abs(max_y+1-top)>1)return false;
        }
        if(name=="base"){base_image=image;base_count=count;}
        if(name=="w2" && (count*4<base_count-8||count*4>base_count+8))return false;
        if(name=="scaled" && image!=base_image)return false;
        if(name=="oversized"){
            for(const auto& v:study_mesh::make_triangle())
                if(study_coordinates::inside_clip_volume(study_projection::clip_for(experiment,v.x,v.y)))return false;
            if(count<=base_count)return false;
        }
        if(capture){const auto path=std::string(capture)+"-"+name+".ppm";if(!save_ppm(gl,path.c_str()))return false;}
        std::printf("%s: %d colored pixels, bounds=(%d,%d)..(%d,%d)\n",experiment.name,count,min_x,min_y,max_x,max_y);
    }
    std::puts("CPU bounds match pixels; w2 shrinks; scaled matches base; all-outside vertices still produce a clipped triangle");
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
           SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER,0)!=0 ||
           SDL_GL_SetAttribute(SDL_GL_RED_SIZE,8)!=0 || SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE,8)!=0 ||
           SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE,8)!=0 || SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE,8)!=0)break;
        window=SDL_CreateWindow("coordinate readback",0,0,width,height,SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN);
        if(!window)break;
        context=SDL_GL_CreateContext(window);
        if(!context||SDL_GL_MakeCurrent(window,context)!=0)break;
        GlApi gl;if(!load(gl,SDL_GL_GetProcAddress))break;
        try{result=checks(gl,argc>1?argv[1]:nullptr)?0:1;}
        catch(const std::exception& e){std::fprintf(stderr,"Coordinate probe exception: %s\n",e.what());}
    }while(false);
    if(result)std::fprintf(stderr,"Coordinate probe failed; SDL diagnostic: %s\n",SDL_GetError());
    if(context)SDL_GL_DeleteContext(context);
    if(window)SDL_DestroyWindow(window);
    SDL_Quit();return result;
}
