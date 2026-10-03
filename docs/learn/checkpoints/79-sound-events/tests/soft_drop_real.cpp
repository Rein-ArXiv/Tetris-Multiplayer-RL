#ifndef SDL_MAIN_HANDLED
#define SDL_MAIN_HANDLED
#endif
#include <SDL.h>
#include "renderer/board_scene.h"
#include "renderer/ghost_view.h"
#include "simulation/ghost.h"
#include "simulation/round.h"
#include "simulation/catalog.h"
#include "tests/locking_oracle.h"
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
static constexpr study_catalog::Kind kinds[]={study_catalog::Kind::I,study_catalog::Kind::J,
    study_catalog::Kind::L,study_catalog::Kind::O,study_catalog::Kind::S,study_catalog::Kind::T,study_catalog::Kind::Z};
struct Case {int w,h,x,y,vw,vh;};
static bool check(SDL_Window* window,const GlApi& gl,const char* capture,Case c,int sample){
    gl.ReadBuffer(Front);
    const bool ended=false;const int kind=sample/5;
    const int ticks[]={0,1,4,5,12};const int count=ticks[sample%5];
    auto round=*study_round::Round::create({},kinds[kind],6);
    const auto initial=study_piece_view::make_visible_mesh(*study_piece::to_board(*round.active()));
    for(int tick=0;tick<count;++tick)
        if(round.tick(0,false,true)==study_round::Step::invalid||!round.active())return false;
    const int expected_active=(count+3)/4+count/6;
    if(round.active()->origin.row!=expected_active)return false;
    const auto& board=round.board();locking_oracle::Board oracle{};
    const auto& active=*round.active();const int col=kind==3?4:3;
    int row=0;while(locking_oracle::fits(oracle,kind,row+1,col))++row;
    const auto hint=round.ghost();if(!hint||hint->piece.origin.row!=row)return false;
    const auto board_mesh=study_board::make_mesh(board);
    const auto piece=study_piece_view::make_visible_mesh(*study_piece::to_board(active));
    const auto ghost=study_piece_view::make_visible_mesh(*study_piece::to_board(hint->piece));
    VertexBuffer bb(gl),ab(gl),gb(gl);VertexArray ba(gl),aa(gl),ga(gl);
    Program empty(gl),filled(gl),cyan(gl),gray(gl);
    if(!study_board_scene::configure(gl)||
       !study_board_scene::link_program(gl,empty,study_board_scene::empty_fragment)||
       !study_board_scene::link_program(gl,filled,study_board_scene::filled_fragment)||
       !study_board_scene::link_program(gl,cyan,study_piece_view::fragment)||
       !study_board_scene::link_program(gl,gray,study_ghost_view::fragment)||
       !bb.upload(board_mesh.vertices)||!ba.configure(bb.name())||
       !ab.upload(initial.vertices.data(),initial.count)||!aa.configure(ab.name())||
       !gb.upload(piece.vertices.data(),piece.count)||!ga.configure(gb.name()))return false;
    const auto active_name=ab.name();
    if(!ab.replace_same_size(piece.vertices.data(),piece.count)||ab.name()!=active_name)return false;
    std::array<study_mesh::Vertex2,24> active_readback{};
    gl.GetBufferSubData(ArrayBuffer,0,sizeof(active_readback),active_readback.data());if(gl.GetError())return false;
    for(std::size_t i=0;i<24;++i)if(active_readback[i].x!=piece.vertices[i].x||active_readback[i].y!=piece.vertices[i].y)return false;
    const auto name=gb.name();
    if(!gb.replace_same_size(ghost.vertices.data(),ghost.count)||gb.name()!=name)return false;
    std::array<study_mesh::Vertex2,24> readback{};
    gl.GetBufferSubData(ArrayBuffer,0,sizeof(readback),readback.data());if(gl.GetError())return false;
    for(std::size_t i=0;i<24;++i)if(readback[i].x!=ghost.vertices[i].x||readback[i].y!=ghost.vertices[i].y)return false;
    int dw=0,dh=0,ww=0,wh=0;SDL_GL_GetDrawableSize(window,&dw,&dh);SDL_GetWindowSize(window,&ww,&wh);
    if(dw!=c.w||dh!=c.h)return false;
    const auto layout=study_letterbox::make_layout({ww,wh},{dw,dh},study_board_scene::logical);if(!layout)return false;
    if(!study_board_scene::render(gl,empty.name(),filled.name(),ba.name(),bb.vertex_count(),board_mesh.empty_vertices,*layout))return false;
    if(!ended&&(!study_piece_view::render(gl,gray.name(),ga.name(),gb.vertex_count())||
               !study_piece_view::render(gl,cyan.name(),aa.name(),ab.vertex_count())))return false;
    std::vector<unsigned char> rgba(static_cast<std::size_t>(dw)*dh*4);
    gl.ReadPixels(0,0,dw,dh,RGBA,UnsignedByte,rgba.data());if(gl.GetError())return false;
    const auto occupied=[&](int r,int c,int base){
        const int y=r-base,x=c-col;
        return y>=0&&y<4&&x>=0&&x<4&&(locking_oracle::masks[kind]&(1u<<(y*4+x)));
    };
    for(int y=0;y<dh;++y)for(int x=0;x<dw;++x){
        const int top=dh-1-y;const bool inside=x>=c.x&&x<c.x+c.vw&&top>=c.y&&top<c.y+c.vh;
        double rgb[]={inside?.03125:0,inside?.0625:0,inside?.09375:0};
        if(inside){
            const double lx=(x+.5-c.x)*320/c.vw,ly=(top+.5-c.y)*240/c.vh;
            if(lx>=110&&lx<210&&ly>=20&&ly<220){
                const int bx=int((lx-110)/10),by=int((ly-20)/10);
                if(lx-(110+bx*10)<9&&ly-(20+by*10)<9){
                    const bool filled=oracle[by*10+bx];rgb[0]=filled?1:.125;rgb[1]=filled?.5:.25;rgb[2]=filled?0:.375;
                    if(!ended&&occupied(by,bx,row))rgb[0]=rgb[1]=rgb[2]=.5;
                    if(!ended&&occupied(by,bx,expected_active)){rgb[0]=0;rgb[1]=.75;rgb[2]=1;}
                }
            }
        }
        for(int ch=0;ch<3;++ch)if(std::abs(rgba[(y*dw+x)*4+ch]-std::lround(rgb[ch]*255))>2){
            std::fprintf(stderr,"soft drop sample%d %dx%d pixel%d,%d ch%d\n",sample,dw,dh,x,top,ch);return false;
        }
    }
    if(capture&&dw==320&&(sample==26||sample==29)){
        const auto path=std::string(capture)+"-sample"+std::to_string(sample)+".ppm";
        FILE* f=std::fopen(path.c_str(),"wb");if(!f)return false;
        bool ok=std::fprintf(f,"P6\n%d %d\n255\n",dw,dh)>0;
        for(int y=dh-1;y>=0;--y)for(int x=0;x<dw;++x)if(std::fwrite(rgba.data()+(y*dw+x)*4,1,3,f)!=3)ok=false;
        if(std::fclose(f)!=0||!ok)return false;
    }
    std::printf("sample%d %dx%d: active and ghost VBO and all framebuffer RGB matched\n",sample,dw,dh);return true;
}
int main(int argc,char** argv){
    SDL_SetMainReady();
    if(SDL_Init(SDL_INIT_VIDEO)!=0)return 1;
    int result=0;
    for(int pattern=0;pattern<35;++pattern)
    for(const Case c : {Case{640,480,0,0,640,480},Case{960,480,160,0,640,480},
                       Case{320,240,0,0,320,240}}) {
    SDL_Window* window=nullptr;SDL_GLContext context=nullptr;int case_result=1;
    do{
        if(SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,3)!=0||SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,3)!=0||
           SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_CORE)!=0 ||
            SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS,SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG)!=0||SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER,0)!=0||
           SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS,0)!=0||SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES,0)!=0)break;
        window=SDL_CreateWindow("soft drop readback",0,0,c.w,c.h,SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN|SDL_WINDOW_RESIZABLE);
        if(!window)break;
        context=SDL_GL_CreateContext(window);if(!context||SDL_GL_MakeCurrent(window,context)!=0)break;
        GlApi gl;if(!load(gl,SDL_GL_GetProcAddress))break;
        try{case_result=check(window,gl,argc>1?argv[1]:nullptr,c,pattern)?0:1;}
        catch(const std::exception& e){std::fprintf(stderr,"board exception: %s\n",e.what());}
    }while(false);
    if(case_result)std::fprintf(stderr,"soft drop readback failed: %s\n",SDL_GetError());
    if(context)SDL_GL_DeleteContext(context);
    if(window)SDL_DestroyWindow(window);
    result |= case_result;
    if(result)break;
    }
    SDL_Quit();return result;
}
