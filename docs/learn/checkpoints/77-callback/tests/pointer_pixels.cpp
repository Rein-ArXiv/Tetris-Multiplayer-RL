#ifndef SDL_MAIN_HANDLED
#define SDL_MAIN_HANDLED
#endif
#include <SDL.h>
#include "renderer/menu_controls.h"
#include "text/paragraph.h"
#include "renderer/board_scene.h"
#include "renderer/letterbox_scene.h"
#include <algorithm>
#include <array>
#include <vector>
#include <cstdio>
#include <cstdlib>
#define CHECK(e) do{if(!(e)){std::fprintf(stderr,"CHECK failed line %d: %s SDL=%s\n",__LINE__,#e,SDL_GetError());std::exit(1);}}while(false)
using namespace study_gl;
int main() {
 SDL_SetMainReady();
 CHECK(SDL_Init(SDL_INIT_VIDEO)==0);
 CHECK(SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,3)==0&&SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,3)==0);
 CHECK(SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_CORE)==0);
 CHECK(SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS,SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG)==0);
 CHECK(SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER,0)==0&&SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS,0)==0);
 auto* window=SDL_CreateWindow("pointer pixel agreement",0,0,1600,1600,SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN);CHECK(window);
 auto context=SDL_GL_CreateContext(window);CHECK(context&&SDL_GL_MakeCurrent(window,context)==0);
 {
 GlApi gl;CHECK(load(gl,SDL_GL_GetProcAddress)&&study_board_scene::configure(gl));
 study_image_quad::ImageQuad quad(gl);CHECK(quad.init());
 study_image::ImageStore images(gl);const unsigned char white[4]={255,255,255,255};
 const auto image=images.create({white,4,1,1});CHECK(image);
 int actual_w=0,actual_h=0,dbl=0;SDL_GL_GetDrawableSize(window,&actual_w,&actual_h);
 CHECK(actual_w>=1200&&actual_h>=1200&&SDL_GL_GetAttribute(SDL_GL_DOUBLEBUFFER,&dbl)==0);gl.ReadBuffer(dbl?Back:Front);
 struct Example{study_letterbox::Size window,drawable;};
 std::size_t compared=0,edges=0,inside=0;
 for(auto e:std::array<Example,5>{{{{320,240},{640,480}},{{501,301},{1002,602}},{{201,500},{402,1000}},{{801,603},{801,603}},{{400,400},{800,1200}}}}){
  const auto layout=study_letterbox::make_layout(e.window,e.drawable,{320,240});CHECK(layout);
  CHECK(study_letterbox_scene::begin(gl,*layout,{0,0,0,1}));
  const auto r=study_menu::start_bounds;
  study_image_quad::Draw request;request.rect={float(r.x),float(r.y),float(r.w),float(r.h)};request.tint={1,0,0,1};
  CHECK(images.draw(quad,image,request));
  const int w=e.drawable.width,h=e.drawable.height;std::vector<unsigned char> pixels(std::size_t(w)*h*4);
  gl.ReadPixels(0,0,w,h,RGBA,UnsignedByte,pixels.data());CHECK(gl.GetError()==0);
  for(int y=0;y<h;++y)for(int x=0;x<w;++x){
   // Compare actual raster coverage with inverse mapping of the SAME drawable
   // sample center. Window positions here are continuous probe points, not OS events.
   const study_letterbox::Point window_point{(x+.5)*e.window.width/w,(y+.5)*e.window.height/h};
   const auto logical=study_letterbox::window_to_logical(*layout,window_point);
   if(logical&&(std::abs(logical->x-r.x)<1e-9||std::abs(logical->x-r.x-r.w)<1e-9||std::abs(logical->y-r.y)<1e-9||std::abs(logical->y-r.y-r.h)<1e-9)){++edges;continue;}
   const bool hit=logical&&study_ui::contains(r,*logical);
   const auto at=(std::size_t(h-1-y)*w+x)*4;
   CHECK((pixels[at]>200)==hit);CHECK(pixels[at+1]==0&&pixels[at+2]==0);
   ++compared;inside+=hit;
  }
 }
 CHECK(inside>1000);
 std::printf("Pointer/raster agreement: %zu sample centers, %zu interior hits, %zu exact-edge exclusions across five independent size pairs; %s\n",compared,inside,edges,gl.GetString(Renderer));
 }
 SDL_GL_DeleteContext(context);SDL_DestroyWindow(window);SDL_Quit();
}
