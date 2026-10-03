// Actual framebuffer readback. This does not measure monitor DPI or presentation.
#ifndef SDL_MAIN_HANDLED
#define SDL_MAIN_HANDLED
#endif
#include <SDL.h>
#include "renderer/board_scene.h"
#include "src/board_example.h"
#include "renderer/vertex_buffer.h"
#include "renderer/vertex_array.h"
#include "renderer/shader.h"
#include "renderer/program.h"
#include <vector>
#include <cstdio>
#include <cmath>
#include <string>
#include <exception>
using namespace study_gl;
struct Case { int w,h,x,y,vw,vh; }; // Independent expected top-left rectangles.
static bool check(SDL_Window* window,const GlApi& gl,const char* capture, Case c, int pattern) {
    GLint major=0,minor=0,profile=0;
    gl.GetIntegerv(MajorVersion,&major);gl.GetIntegerv(MinorVersion,&minor);
    gl.GetIntegerv(ContextProfileMask,&profile);
    const auto* renderer=gl.GetString(Renderer);
    if(gl.GetError()||!renderer||!(major>3||(major==3&&minor>=3))||!(profile&CoreProfileBit))return false;
    int samples=-1,doublebuffer=-1;
    if(SDL_GL_GetAttribute(SDL_GL_MULTISAMPLEBUFFERS,&samples)!=0 || samples!=0 ||
       SDL_GL_GetAttribute(SDL_GL_DOUBLEBUFFER,&doublebuffer)!=0)return false;
    gl.ReadBuffer(doublebuffer?Back:Front);
    std::printf("Actual GL %d.%d Core %s; single-sample readback, doublebuffer=%d\n",
        major,minor,reinterpret_cast<const char*>(renderer),doublebuffer);
    auto board=make_example_board();
    if(pattern==1)board.clear();
    if(pattern==2)for(int r=0;r<20;++r)for(int col=0;col<10;++col)(void)board.set(r,col,study_grid::Cell::filled);
    const auto state_before=board.cells();
    const auto mesh=study_board::make_mesh(board);
    VertexBuffer buffer(gl);VertexArray vao(gl);Program empty(gl),filled(gl);
    if(!buffer.upload(mesh.vertices)||!vao.configure(buffer.name()))return false;
    if(!study_board_scene::link_program(gl,empty,study_board_scene::empty_fragment) ||
       !study_board_scene::link_program(gl,filled,study_board_scene::filled_fragment) ||
       !study_board_scene::configure(gl))return false;
    {
        int dw=0,dh=0,ww=0,wh=0;
        SDL_GL_GetDrawableSize(window,&dw,&dh);SDL_GetWindowSize(window,&ww,&wh);
        if(dw!=c.w||dh!=c.h){std::fprintf(stderr,"probe requires requested pixel sizes; got %dx%d\n",dw,dh);return false;}
        // Dirty state/contents from an earlier pass must not leak into the bars.
        gl.Disable(ScissorTest);gl.ClearColor(1,0,1,1);gl.Clear(ColorBufferBit);
        gl.Scissor(0,0,1,1);gl.Enable(ScissorTest);gl.Viewport(0,0,1,1);
        const auto layout=study_letterbox::make_layout({ww,wh},{dw,dh},study_board_scene::logical);
        if(!layout)return false;
        // Invalid group metadata must fail before clearing/submitting.
        if(study_board_scene::render(gl,empty.name(),filled.name(),vao.name(),buffer.vertex_count(),1,*layout))return false;
        unsigned char unchanged[4]{};gl.ReadPixels(0,0,1,1,RGBA,UnsignedByte,unchanged);
        if(gl.GetError() || unchanged[0]!=255 || unchanged[1]!=0 || unchanged[2]!=255)return false;
        if(!study_board_scene::render(gl,empty.name(),filled.name(),vao.name(),buffer.vertex_count(),mesh.empty_vertices,*layout))return false;
        GLint actual[4]{};gl.GetIntegerv(ViewportState,actual);
        if(actual[0]!=c.x||actual[1]!=c.h-c.y-c.vh||actual[2]!=c.vw||actual[3]!=c.vh)return false;
        std::vector<unsigned char> rgba(static_cast<std::size_t>(dw)*dh*4);
        gl.ReadPixels(0,0,dw,dh,RGBA,UnsignedByte,rgba.data());if(gl.GetError())return false;
        for(int y=0;y<dh;++y)for(int x=0;x<dw;++x){
            const int top=dh-1-y;
            const bool inside=x>=c.x&&x<c.x+c.vw&&top>=c.y&&top<c.y+c.vh;
            double expected[3]={inside?0.03125:0,inside?0.0625:0,inside?0.09375:0};
            if(inside){
                // Independent expected positions in logical units (top-left).
                const double lx=(x+0.5-c.x)*320/c.vw;
                const double ly=(top+0.5-c.y)*240/c.vh;
                if(lx>=110 && lx<210 && ly>=20 && ly<220){
                    const int col=static_cast<int>((lx-110)/10),row=static_cast<int>((ly-20)/10);
                    if(lx-(110+col*10)<9 && ly-(20+row*10)<9){
                        const bool occupied=pattern==2 || (pattern==0 &&
                            ((row==0&&col==0)||(row==1&&col==2)||(row==19&&col==9)));
                        expected[0]=occupied?1:0.125;
                        expected[1]=occupied?0.5:0.25;
                        expected[2]=occupied?0:0.375;
                    }
                }
            }
            for(int k=0;k<3;++k)if(std::abs(rgba[(y*dw+x)*4+k]-std::lround(255*expected[k]))>2){
                std::fprintf(stderr,"%dx%d pixel %d,%d ch%d expected%.2f got%u\n",dw,dh,x,top,k,255*expected[k],rgba[(y*dw+x)*4+k]);return false;
            }
        }
        if(capture){
            const std::string path=std::string(capture)+"-p"+std::to_string(pattern)+"-"+std::to_string(dw)+"x"+std::to_string(dh)+".ppm";
            FILE* f=std::fopen(path.c_str(),"wb");if(!f)return false;
            bool ok=std::fprintf(f,"P6\n%d %d\n255\n",dw,dh)>0;
            for(int y=dh-1;y>=0;--y)for(int x=0;x<dw;++x)
                if(std::fwrite(rgba.data()+(y*dw+x)*4,1,3,f)!=3)ok=false;
            if(std::fclose(f)!=0)ok=false;
            if(!ok)return false;
        }
        std::printf("drawable %dx%d: all pixels matched bars, background, board cells and gaps\n",dw,dh);
    }
    return board.cells()==state_before;
}
int main(int argc,char** argv){
    SDL_SetMainReady(); // Explicit console entry (SDL_MAIN_HANDLED).
    if(SDL_Init(SDL_INIT_VIDEO)!=0)return 1;
    int result=0;
    for(int pattern=0;pattern<3;++pattern)
    for(const Case c : {Case{640,480,0,0,640,480},Case{960,480,160,0,640,480},
                       Case{640,960,0,240,640,480},Case{320,240,0,0,320,240}}) {
    SDL_Window* window=nullptr;SDL_GLContext context=nullptr;int case_result=1;
    do{
        if(SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,3)!=0||SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,3)!=0||
           SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_CORE)!=0 ||
            SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS,SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG)!=0||SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER,0)!=0||
           SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS,0)!=0||SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES,0)!=0)break;
        window=SDL_CreateWindow("board readback",0,0,c.w,c.h,SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN|SDL_WINDOW_RESIZABLE);
        if(!window)break;
        context=SDL_GL_CreateContext(window);if(!context||SDL_GL_MakeCurrent(window,context)!=0)break;
        GlApi gl;if(!load(gl,SDL_GL_GetProcAddress))break;
        try{case_result=check(window,gl,argc>1?argv[1]:nullptr,c,pattern)?0:1;}
        catch(const std::exception& e){std::fprintf(stderr,"board exception: %s\n",e.what());}
    }while(false);
    if(case_result)std::fprintf(stderr,"board readback failed: %s\n",SDL_GetError());
    if(context)SDL_GL_DeleteContext(context);
    if(window)SDL_DestroyWindow(window);
    result |= case_result;
    if(result)break;
    }
    SDL_Quit();return result;
}
