// Actual framebuffer readback. This does not measure monitor DPI or presentation.
#ifndef SDL_MAIN_HANDLED
#define SDL_MAIN_HANDLED
#endif
#include <SDL.h>
#include "renderer/board_scene.h"
#include "src/spawn_example.h"
#include "renderer/end_marker.h"
#include "simulation/catalog.h"
#include "simulation/movement.h"
#include "simulation/collision.h"
#include "simulation/round.h"
#include "tests/locking_oracle.h"
#include "renderer/piece_scene.h"
#include "renderer/next_preview.h"
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
static bool check(SDL_Window* window,const GlApi& gl,const char* capture,Case c,int pattern,int scenario){
    gl.ReadBuffer(Front);
    const auto kind=study_catalog::definitions[pattern].kind;
    const auto fixture=scenario==0?spawn_example::Scenario::initial_blocked:scenario==1?spawn_example::Scenario::next_blocked:spawn_example::Scenario::clear_rescue;
    auto round=*study_round::Round::create(*spawn_example::make(kind,fixture),*study_next::ScriptedSource::cycle(kind),1);
    locking_oracle::Board expected{};
    const int column=pattern==3?4:3;
    int first=-1,bottom=-1;
    for(int i=0;i<16;++i)if(locking_oracle::masks[pattern]&(1u<<i)){
        if(first<0)first=i;
        if(bottom<0||i/4>bottom/4)bottom=i;
    }
    if(scenario==0)expected[first/4*10+column+first%4]=true;
    else{
        expected[(bottom/4+1)*10+column+bottom%4]=true;
        if(scenario==2){
            for(int r=first/4;r<=bottom/4;++r)for(int col=0;col<10;++col)expected[r*10+col]=true;
            for(int i=0;i<16;++i)if(locking_oracle::masks[pattern]&(1u<<i))expected[i/4*10+column+i%4]=false;
        }
    }
    VertexBuffer board_buffer(gl),piece_buffer(gl),preview_buffer(gl),end_buffer(gl);
    VertexArray board_array(gl),piece_array(gl),preview_array(gl),end_array(gl);
    Program empty(gl),filled(gl),cyan(gl),red(gl);
    if(!study_board_scene::configure(gl)||
       !study_board_scene::link_program(gl,empty,study_board_scene::empty_fragment)||
       !study_board_scene::link_program(gl,filled,study_board_scene::filled_fragment)||
       !study_board_scene::link_program(gl,cyan,study_piece_view::fragment)||
       !study_board_scene::link_program(gl,red,study_end_view::fragment))return false;
    const auto end_mesh=study_end_view::make_mesh();
    if(!end_buffer.upload(end_mesh)||!end_array.configure(end_buffer.name()))return false;
    const auto initial_mesh=study_board::make_mesh(round.board());
    const auto initial_preview=*study_next_view::make_mesh(round.next());
    if(!board_buffer.upload(initial_mesh.vertices)||!board_array.configure(board_buffer.name())||
       !preview_buffer.upload(initial_preview.vertices.data(),initial_preview.count)||!preview_array.configure(preview_buffer.name()))return false;
    if(round.active()){
        const auto mesh=study_piece_view::make_visible_mesh(*study_piece::to_board(*round.active()));
        if(!piece_buffer.upload(mesh.vertices.data(),mesh.count)||!piece_array.configure(piece_buffer.name()))return false;
    }
    for(int pose=0;pose<2;++pose){
        if(pose){
            const auto step=round.tick(0);
            if(step!=(scenario==0?study_round::Step::stopped:scenario==1?study_round::Step::game_over:study_round::Step::locked))return false;
            if(scenario){
                locking_oracle::fill(expected,pattern,0,column);
                if(scenario==2){
                    locking_oracle::Board filtered{};int write=19;
                    for(int r=19;r>=0;--r){
                        bool full=true;for(int col=0;col<10;++col)full=full&&expected[r*10+col];
                        if(full)continue;
                        for(int col=0;col<10;++col)filtered[write*10+col]=expected[r*10+col];
                        --write;
                    }
                    expected=filtered;
                }
                const auto mesh=study_board::make_mesh(round.board());
                const auto preview=*study_next_view::make_mesh(round.next());
                if(!board_buffer.replace_same_size(mesh.vertices.data(),mesh.vertices.size())||
                   !preview_buffer.replace_same_size(preview.vertices.data(),preview.count))return false;
                if(round.active()){
                    const auto overlay=study_piece_view::make_visible_mesh(*study_piece::to_board(*round.active()));
                    if(!piece_buffer.replace_same_size(overlay.vertices.data(),overlay.count))return false;
                }
            }
        }
        const bool ended=scenario==0||(scenario==1&&pose==1);
        if(round.finished()!=ended||bool(round.active())==ended)return false;
        const int current=(pattern+(pose&&scenario?1:0))%7;
        for(int i=0;i<200;++i)if((round.board().cells()[i]==study_grid::Cell::filled)!=expected[i])return false;
        int dw=0,dh=0,ww=0,wh=0;SDL_GL_GetDrawableSize(window,&dw,&dh);SDL_GetWindowSize(window,&ww,&wh);
        if(dw!=c.w||dh!=c.h)return false;
        const auto layout=study_letterbox::make_layout({ww,wh},{dw,dh},study_board_scene::logical);if(!layout)return false;
        const auto board_mesh=study_board::make_mesh(round.board());
        if(!study_board_scene::render(gl,empty.name(),filled.name(),board_array.name(),board_buffer.vertex_count(),board_mesh.empty_vertices,*layout)||
           (round.active()&&!study_piece_view::render(gl,cyan.name(),piece_array.name(),piece_buffer.vertex_count()))||
           !study_next_view::render(gl,cyan.name(),preview_array.name(),preview_buffer.vertex_count())||
           (ended&&!study_end_view::render(gl,red.name(),end_array.name(),end_buffer.vertex_count())))return false;
        std::vector<unsigned char> rgba(static_cast<std::size_t>(dw)*dh*4);
        gl.ReadPixels(0,0,dw,dh,RGBA,UnsignedByte,rgba.data());if(gl.GetError())return false;
        unsigned ambiguous=0;
        for(int y=0;y<dh;++y)for(int x=0;x<dw;++x){
            const int top=dh-1-y;
            const bool inside=x>=c.x&&x<c.x+c.vw&&top>=c.y&&top<c.y+c.vh;
            double rgb[3]={inside?0.03125:0,inside?0.0625:0,inside?0.09375:0};bool edge=false;
            if(inside){
                const double lx=(x+0.5-c.x)*320/c.vw,ly=(top+0.5-c.y)*240/c.vh;
                if(lx>=110&&lx<210&&ly>=20&&ly<220){
                    const int col=int((lx-110)/10),row=int((ly-20)/10);
                    if(lx-(110+col*10)<9&&ly-(20+row*10)<9){
                        const bool occupied=expected[row*10+col];rgb[0]=occupied?1:.125;rgb[1]=occupied?.5:.25;rgb[2]=occupied?0:.375;
                        const int local=col-(current==3?4:3);
                        if(!ended&&row<4&&local>=0&&local<4&&(locking_oracle::masks[current]&(1u<<(row*4+local)))){rgb[0]=0;rgb[1]=.75;rgb[2]=1;}
                    }
                }
                for(int slot=0;slot<3;++slot){
                    const double px=lx-244,py=ly-(40+60*slot);
                    if(px>=0&&px<24&&py>=0&&py<24){const int col=int(px/6),row=int(py/6);
                        if(px-col*6<5&&py-row*6<5&&(locking_oracle::masks[(current+slot+1)%7]&(1u<<(row*4+col)))){rgb[0]=0;rgb[1]=.75;rgb[2]=1;}
                    }
                }
                if(ended){
                    const double sum=lx+ly,diff=lx-ly;
                    const bool a=sum>=116&&sum<=184&&diff>=-56&&diff<=-44;
                    const bool b=sum>=144&&sum<=156&&diff>=-84&&diff<=-16;
                    // Exact diagonal-edge samples may belong to either adjoining
                    // primitive under rasterization rounding. Only these edge
                    // samples admit either background or marker, never arbitrary RGB.
                    edge=(a&&(sum==116||sum==184||diff==-56||diff==-44))||(b&&(sum==144||sum==156||diff==-84||diff==-16));
                    if(a||b){rgb[0]=1;rgb[1]=.125;rgb[2]=.125;}
                }
            }
            bool matches=true,background=true;
            const double bg[]={.03125,.0625,.09375};
            for(int k=0;k<3;++k){
                matches=matches&&std::abs(rgba[(y*dw+x)*4+k]-std::lround(255*rgb[k]))<=2;
                background=background&&std::abs(rgba[(y*dw+x)*4+k]-std::lround(255*bg[k]))<=2;
            }
            if(edge)++ambiguous;
            if(!matches&&!(edge&&background)){std::fprintf(stderr,"pixel kind%d scenario%d pose%d %dx%d (%d,%d)\n",pattern,scenario,pose,dw,dh,x,top);return false;}
        }
        if(capture&&pattern==3&&dw==320&&pose==1){
            const std::string path=std::string(capture)+"-scenario"+std::to_string(scenario)+".ppm";
            FILE* f=std::fopen(path.c_str(),"wb");if(!f)return false;
            bool ok=std::fprintf(f,"P6\n%d %d\n255\n",dw,dh)>0;
            for(int y=dh-1;y>=0;--y)for(int x=0;x<dw;++x)if(std::fwrite(rgba.data()+(y*dw+x)*4,1,3,f)!=3)ok=false;
            if(std::fclose(f)!=0||!ok)return false;
        }
        std::printf("kind%d scenario%d pose%d %dx%d framebuffer matched; %u diagonal-edge samples checked against two allowed colors\n",pattern,scenario,pose,dw,dh,ambiguous);
    }
    return true;
}
int main(int argc,char** argv){
    SDL_SetMainReady(); // Explicit console entry (SDL_MAIN_HANDLED).
    if(SDL_Init(SDL_INIT_VIDEO)!=0)return 1;
    int result=0;
    for(int pattern=0;pattern<7;++pattern)
    for(int scenario=0;scenario<3;++scenario)
    for(const Case c : {Case{640,480,0,0,640,480},Case{960,480,160,0,640,480},
                       Case{320,240,0,0,320,240}}) {
    SDL_Window* window=nullptr;SDL_GLContext context=nullptr;int case_result=1;
    do{
        if(SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,3)!=0||SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,3)!=0||
           SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_CORE)!=0 ||
            SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS,SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG)!=0||SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER,0)!=0||
           SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS,0)!=0||SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES,0)!=0)break;
        window=SDL_CreateWindow("end-state readback",0,0,c.w,c.h,SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN|SDL_WINDOW_RESIZABLE);
        if(!window)break;
        context=SDL_GL_CreateContext(window);if(!context||SDL_GL_MakeCurrent(window,context)!=0)break;
        GlApi gl;if(!load(gl,SDL_GL_GetProcAddress))break;
        try{case_result=check(window,gl,argc>1?argv[1]:nullptr,c,pattern,scenario)?0:1;}
        catch(const std::exception& e){std::fprintf(stderr,"board exception: %s\n",e.what());}
    }while(false);
    if(case_result)std::fprintf(stderr,"end-state readback failed: %s\n",SDL_GetError());
    if(context)SDL_GL_DeleteContext(context);
    if(window)SDL_DestroyWindow(window);
    result |= case_result;
    if(result)break;
    }
    SDL_Quit();return result;
}
