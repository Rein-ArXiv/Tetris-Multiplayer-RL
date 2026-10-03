#ifndef SDL_MAIN_HANDLED
#define SDL_MAIN_HANDLED
#endif
#include <SDL.h>
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
int main(int argc,char** argv) {
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
 const auto fallback_pixels=study_texture::make_play_badge();
 const auto fallback=images.create({fallback_pixels.data(),fallback_pixels.size(),8,8});CHECK(fallback);
 study_art::ArtSet art;int calls=0;
 CHECK(art.init([&](const std::string& path){++calls;if(path=="assets/bot.png")return study_image::Handle(0);return images.load(std::filesystem::path(argv[1])/path);},fallback));
 CHECK(calls==3&&images.count()==3);
 const auto* player=art.resolve("player");const auto* rook=art.resolve("rook");CHECK(player&&rook);
 CHECK(player->icon==player->portrait&&!player->icon_fallback&&!player->portrait_fallback);
 CHECK(rook->icon==fallback&&rook->icon_fallback&&!rook->portrait_fallback);
 study_menu::Preferences prefs;CHECK(prefs.select_character("rook"));
 const auto layout=study_letterbox::make_layout({640,480},{640,480},{320,240});CHECK(layout);
 CHECK(study_letterbox_scene::begin(gl,*layout,{0,0,0,1}));
 CHECK(study_art::draw_contained(images,quad,rook->icon,{20,20,48,48}));
 CHECK(study_art::draw_contained(images,quad,rook->portrait,{114,168,172,56}));
 CHECK(prefs.character_id()=="rook"&&gl.GetError()==0);
 int dbl=0;CHECK(SDL_GL_GetAttribute(SDL_GL_DOUBLEBUFFER,&dbl)==0);gl.ReadBuffer(dbl?Back:Front);
 std::size_t samples=0;
 for(int variant=0;variant<3;++variant){
  const int iw=variant==0?4:variant==1?2:1000,ih=variant==0?2:variant==1?4:1;
  std::vector<unsigned char> green(std::size_t(iw)*ih*4,0);
  for(std::size_t i=0;i<green.size();i+=4){green[i+1]=255;green[i+3]=255;}
  const auto handle=images.create({green.data(),green.size(),iw,ih});CHECK(handle);
  CHECK(study_letterbox_scene::begin(gl,*layout,{0,0,0,1}));
  CHECK(study_art::draw_contained(images,quad,handle,{20,20,40,40}));
  std::vector<unsigned char> frame(640*480*4);gl.ReadPixels(0,0,640,480,RGBA,UnsignedByte,frame.data());CHECK(gl.GetError()==0);
  for(int y=0;y<480;++y)for(int x=0;x<640;++x){
   const bool expected=variant==0?(x>=40&&x<120&&y>=60&&y<100):variant==1?(x>=60&&x<100&&y>=40&&y<120):false;
   const auto at=(std::size_t(479-y)*640+x)*4;CHECK(frame[at]==0&&frame[at+2]==0&&frame[at+1]==(expected?255:0));++samples;
  }
  CHECK(images.unload(handle));
 }
 const auto stale=player->icon;CHECK(images.unload(stale));CHECK(!study_art::draw_contained(images,quad,stale,{20,20,40,40}));
 images.clear();CHECK(images.count()==0);
 std::printf("Character GL: file dedup/failed icon fallback/independent portrait/stable ID; %zu pixels for wide/tall/sub-unit omission; stale borrowed handle rejected; %s\n",samples,gl.GetString(Renderer));
 }
 SDL_GL_DeleteContext(context);SDL_DestroyWindow(window);SDL_Quit();
}
