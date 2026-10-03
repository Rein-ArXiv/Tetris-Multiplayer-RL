#ifndef SDL_MAIN_HANDLED
#define SDL_MAIN_HANDLED
#endif
#include <SDL.h>
#include "presentation/idle_animation.h"
#include <limits>
#include "renderer/menu_controls.h"
#include "content/art_set.h"
#include "renderer/session_icons.h"
#include "presentation/art_view.h"
#include <vector>
#include "text/paragraph.h"
#include "renderer/board_scene.h"
#include "renderer/letterbox_scene.h"
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#define CHECK(e) do{if(!(e)){std::fprintf(stderr,"CHECK failed line %d: %s SDL=%s\n",__LINE__,#e,SDL_GetError());std::exit(1);}}while(false)
using namespace study_gl;
int main(int argc,char**) {
    SDL_SetMainReady(); // Explicit console entry (SDL_MAIN_HANDLED).
 CHECK(argc==2&&SDL_Init(SDL_INIT_VIDEO)==0);
 CHECK(SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,3)==0&&SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,3)==0);
 CHECK(SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_CORE)==0);
 CHECK(SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS,SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG)==0);
 CHECK(SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER,0)==0&&SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS,0)==0);
 auto* window=SDL_CreateWindow("glyph",0,0,1280,960,SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN);CHECK(window);
 auto context=SDL_GL_CreateContext(window);CHECK(context&&SDL_GL_MakeCurrent(window,context)==0);
 {
 GlApi gl;CHECK(load(gl,SDL_GL_GetProcAddress)&&study_board_scene::configure(gl));
 study_image_quad::ImageQuad quad(gl);CHECK(quad.init());
 study_image::ImageStore images(gl);
 const unsigned char white[4]={255,255,255,255};
 const auto handle=images.create({white,4,1,1});CHECK(handle);
 const auto layout=study_letterbox::make_layout({640,480},{640,480},{320,240});CHECK(layout);
 int dbl=0;CHECK(SDL_GL_GetAttribute(SDL_GL_DOUBLEBUFFER,&dbl)==0);gl.ReadBuffer(dbl?Back:Front);
 study_idle::Animation animation(17);std::size_t samples=0;
 for(int frame=0;frame<8;++frame){
  CHECK(animation.advance(.0625,true));const auto values=animation.sample(frame!=7);
  for(double alpha:{values.border_alpha,values.portrait_alpha}){
   CHECK(study_letterbox_scene::begin(gl,*layout,{0,0,0,1}));
   CHECK(study_art::draw_contained(images,quad,handle,{20,20,40,40},alpha));
   std::vector<unsigned char> pixels(640*480*4);gl.ReadPixels(0,0,640,480,RGBA,UnsignedByte,pixels.data());CHECK(gl.GetError()==0);
   const int expected=int(std::lround(float(alpha)*255));
   for(int y=0;y<480;++y)for(int x=0;x<640;++x){
    const bool inside=x>=40&&x<120&&y>=40&&y<120;
    const auto at=(std::size_t(479-y)*640+x)*4;
    for(int channel=0;channel<3;++channel)CHECK(std::abs(int(pixels[at+channel])-(inside?expected:0))<=(inside?1:0));
    ++samples;
   }
  }
  study_image_quad::Draw rotated;rotated.rect={28,148,40,40};rotated.angle_degrees=float(values.decoration_degrees);
  rotated.tint={.8f,.6f,1,.5f};CHECK(images.draw(quad,handle,rotated)&&gl.GetError()==0);
 }
 for(double bad:{-1.,1.1,std::numeric_limits<double>::quiet_NaN(),std::numeric_limits<double>::infinity()})CHECK(!study_art::draw_contained(images,quad,handle,{20,20,40,40},bad));
 std::printf("Idle GL: %zu alpha/coverage pixels (RGB tolerance 1 inside), disabled static frame and rotation submissions; %s\n",samples,gl.GetString(Renderer));
 }
 SDL_GL_DeleteContext(context);SDL_DestroyWindow(window);SDL_Quit();
}
