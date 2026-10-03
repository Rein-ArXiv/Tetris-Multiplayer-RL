// Offscreen framebuffer equivalence, not a monitor/presentation measurement.
#ifndef SDL_MAIN_HANDLED
#define SDL_MAIN_HANDLED
#endif
#include <SDL.h>
#include "renderer/flush_device.h"
#include "renderer/flush_scene.h"
#include "renderer/batch_scene.h"
#include "renderer/board_scene.h"
#include "renderer/vertex_buffer.h"
#include "renderer/vertex_array.h"
#include "renderer/ghost_view.h"
#include "client/game.h"
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <cmath>
#define CHECK(e) do{if(!(e)){std::fprintf(stderr,"CHECK failed line %d: %s SDL=%s\n",__LINE__,#e,SDL_GetError());std::exit(1);}}while(false)
using namespace study_gl;
static unsigned draw_calls=0;
static void mesh(const GlApi& gl,const study_mesh::Vertex2* vertices,std::size_t count,const char* fragment){
 if(!count)return;
 VertexBuffer buffer(gl);VertexArray array(gl);Program program(gl);
 CHECK(buffer.upload(vertices,count)&&array.configure(buffer.name()));
 CHECK(study_board_scene::link_program(gl,program,fragment));
 CHECK(submit_triangles(gl,program.name(),array.name(),static_cast<GLsizei>(count),0,static_cast<GLsizei>(count)));
 ++draw_calls;
}
static void legacy(const GlApi& gl,const study_game::GameView& view){
 const auto board=study_board::make_mesh(view.board);
 mesh(gl,board.vertices.data(),board.empty_vertices,study_board_scene::empty_fragment);
 mesh(gl,board.vertices.data()+board.empty_vertices,board.vertices.size()-board.empty_vertices,study_board_scene::filled_fragment);
 if(view.ghost){const auto cells=study_piece::to_board(view.ghost->piece);CHECK(cells);const auto m=study_piece_view::make_visible_mesh(*cells);mesh(gl,m.vertices.data(),m.count,study_ghost_view::fragment);}
 if(view.active){const auto cells=study_piece::to_board(*view.active);CHECK(cells);const auto m=study_piece_view::make_visible_mesh(*cells);mesh(gl,m.vertices.data(),m.count,study_piece_view::fragment);}
 const auto next=study_next_view::make_mesh(view.next);CHECK(next);mesh(gl,next->vertices.data(),next->count,study_piece_view::fragment);
 if(view.end_reason!=study_round::EndReason::none){const auto end=study_end_view::make_mesh();mesh(gl,end.data(),end.size(),study_end_view::fragment);}
 const auto score=study_score_view::make_mesh(view.score);mesh(gl,score.vertices.data(),score.count,study_score_view::fragment);
}
int main(){
    SDL_SetMainReady(); // Explicit console entry (SDL_MAIN_HANDLED).
 CHECK(SDL_Init(SDL_INIT_VIDEO)==0);
 CHECK(SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,3)==0&&SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,3)==0);
 CHECK(SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_CORE)==0);
 CHECK(SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS,SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG)==0);
 CHECK(SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER,0)==0&&SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS,0)==0);
 SDL_Window* window=SDL_CreateWindow("batch equality",0,0,320,240,SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN);CHECK(window);
 SDL_GLContext context=SDL_GL_CreateContext(window);CHECK(context);CHECK(SDL_GL_MakeCurrent(window,context)==0);
 {
 GlApi gl;CHECK(load(gl,SDL_GL_GetProcAddress));GLint major=0,minor=0,profile=0;
 gl.GetIntegerv(MajorVersion,&major);gl.GetIntegerv(MinorVersion,&minor);gl.GetIntegerv(ContextProfileMask,&profile);
 CHECK((major>3||(major==3&&minor>=3))&&(profile&CoreProfileBit)&&gl.GetError()==0);
 int w=0,h=0,dbl=0;SDL_GL_GetDrawableSize(window,&w,&h);CHECK(SDL_GL_GetAttribute(SDL_GL_DOUBLEBUFFER,&dbl)==0);
 const auto layout=study_letterbox::make_layout({320,240},{w,h},study_board_scene::logical);CHECK(layout);
 CHECK(study_board_scene::configure(gl));study_batch::Device device(gl);CHECK(device.init());
 std::vector<unsigned char> a(w*h*4),b(a.size());gl.ReadBuffer(dbl?Back:Front);
 auto compare=[&](const study_game::GameView& view){
   study_batch::Batch batch;CHECK(study_batch::scene(batch,view));
   CHECK(study_letterbox_scene::begin(gl,*layout,{.03125,.0625,.09375,1}));draw_calls=0;legacy(gl,view);
   gl.ReadPixels(0,0,w,h,RGBA,UnsignedByte,a.data());CHECK(gl.GetError()==0);
   CHECK(study_letterbox_scene::begin(gl,*layout,{.03125,.0625,.09375,1}));study_flush::GlSink sink{gl,device};study_flush::Stream<study_flush::GlSink> stream(sink,study_flush::full_clip(*layout));const auto clip=study_flush::project_clip(*layout,{110,20,100,200});CHECK(clip&&study_flush::scene(stream,view,study_flush::full_clip(*layout),*clip)&&stream.finish());CHECK(stream.statistics().draws==2);
   gl.ReadPixels(0,0,w,h,RGBA,UnsignedByte,b.data());CHECK(gl.GetError()==0);
   unsigned worst=0;for(std::size_t i=0;i<a.size();++i){unsigned delta=static_cast<unsigned>(std::abs(int(a[i])-int(b[i])));if(delta>worst)worst=delta;CHECK(delta<=1);}
   std::printf("%zu vertices: legacy %u draws -> clipped stream 2; all pixels max channel delta=%u\n",batch.size(),draw_calls,worst);
 };
 auto initial=study_round::Round::create_seeded(study_grid::Grid{},1);CHECK(initial);study_game::Game game(*initial);
 for(int i=0;i<6;++i){auto view=game.view();CHECK(view);if(i==5)view->score=UINT64_MAX;compare(*view);CHECK(game.advance(.017,{false,false,false,false,true,false}));}
 study_grid::Grid tall;for(int c=1;c<10;++c)CHECK(tall.set(4,c,study_grid::Cell::filled));auto fast=study_round::Round::create(tall,study_catalog::Kind::T,1);CHECK(fast);study_game::Game end(*fast);CHECK(end.advance(.1,{}));auto last=end.view();CHECK(last&&end.round().finished());compare(*last);
 // Menu uses the same old triangle positions and orange program.
 study_batch::Batch menu;CHECK(study_batch::menu(menu));const study_mesh::Vertex2 points[]={{-.25f,-.4f},{.35f,0},{-.25f,.4f}};
 CHECK(study_letterbox_scene::begin(gl,*layout,{.04,.07,.12,1}));mesh(gl,points,3,study_board_scene::filled_fragment);gl.ReadPixels(0,0,w,h,RGBA,UnsignedByte,a.data());CHECK(gl.GetError()==0);
 CHECK(study_letterbox_scene::begin(gl,*layout,{.04,.07,.12,1}));study_flush::GlSink menu_sink{gl,device};study_flush::Stream<study_flush::GlSink> menu_stream(menu_sink,study_flush::full_clip(*layout));CHECK(study_flush::menu(menu_stream)&&menu_stream.finish()&&menu_stream.statistics().draws==1);gl.ReadPixels(0,0,w,h,RGBA,UnsignedByte,b.data());CHECK(gl.GetError()==0);CHECK(a==b);

 // Deliberately overlapping geometry: full-surface quads must use each old clip.
 const study_mesh::Vertex2 quad[]={{-1,-1},{1,-1},{1,1},{-1,-1},{1,1},{-1,1}};
 for(study_letterbox::Size logical:{study_letterbox::Size{320,240},{319,239},{240,320}}){
  const auto l=study_letterbox::make_layout({320,240},{w,h},logical);CHECK(l);
  const auto first=study_flush::project_clip(*l,{logical.width/8,logical.height/8,logical.width/2,logical.height/2});
  const auto second=study_flush::project_clip(*l,{logical.width/3,logical.height/3,logical.width/2,logical.height/2});CHECK(first&&second);
  const study_flush::Clip clips[]={*first,*second,*first};
  const study_batch::Color colors[]={{1,0,0,1},{0,1,0,1},{0,0,1,1}};
  CHECK(study_letterbox_scene::begin(gl,*l,{0,0,0,1}));
  study_flush::GlSink sink{gl,device};study_flush::Stream<study_flush::GlSink> stream(sink,study_flush::full_clip(*l));
  for(unsigned k=0;k<3;++k)CHECK(stream.set_clip(clips[k])&&stream.append(quad,6,colors[k]));
  CHECK(stream.finish()&&stream.statistics().draws==3);
  gl.ReadPixels(0,0,w,h,RGBA,UnsignedByte,a.data());CHECK(gl.GetError()==0);
  for(int y=0;y<h;++y)for(int x=0;x<w;++x){unsigned char rgb[]={0,0,0};
   for(unsigned k=0;k<3;++k){auto c=clips[k];if(x>=c.x&&x<c.x+c.width&&y>=c.y&&y<c.y+c.height){rgb[0]=k==0?255:0;rgb[1]=k==1?255:0;rgb[2]=k==2?255:0;}}
   const auto i=static_cast<std::size_t>(y*w+x)*4;CHECK(a[i]==rgb[0]&&a[i+1]==rgb[1]&&a[i+2]==rgb[2]);
  }
 }
 std::printf("Menu equality; three overlapping clip sequences exact; GL renderer: %s\n",gl.GetString(Renderer));
 }
 SDL_GL_DeleteContext(context);SDL_DestroyWindow(window);SDL_Quit();
}
