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
 study_font::GlyphCache cpu;CHECK(cpu.load_trusted(std::filesystem::u8path(argv[1])));
 study_atlas::GlyphAtlas atlas(gl,512,512);CHECK(atlas.init());
 study_text::GpuGlyphCache cache(atlas);
 study_menu_render::Labels labels;
 study_image_quad::ImageQuad text(gl);CHECK(text.init(study_image_quad::Sampling::coverage));
 study_rounded::RoundedQuad rounded(gl);CHECK(rounded.init());
 study_image::ImageStore images(gl);const unsigned char white[4]={255,255,255,255};
 const auto panel=images.create({white,4,1,1});CHECK(panel);
 std::size_t cases=0;
 for(double density:{1.,2.,3.375}) {
  labels.reset();cache.clear();cpu.clear();CHECK(atlas.clear());
  CHECK(labels.init(cpu,cache,density));CHECK(!labels.init(cpu,cache,density));
  const int w=int(320*density),h=int(240*density);
  const auto layout=study_letterbox::make_layout({w,h},{w,h},{320,240});CHECK(layout);
  CHECK(study_letterbox_scene::begin(gl,*layout,{.125,.25,.375,1}));
  for(int i=0;i<int(study_menu_render::Label::count);++i)
    CHECK(labels.centered(study_menu_render::Label(i),text,160,60+float(i)*20));
  CHECK(!labels.centered(study_menu_render::Label::count,text,160,80));
  for(std::size_t i=0;i<study_character_art::characters.size();++i) CHECK(labels.character(i,text,160,160+float(i)*20));
  CHECK(!labels.character(study_character_art::characters.size(),text,160,80));
  for(int decoration=0;decoration<2;++decoration)for(int badge=0;badge<2;++badge)for(auto focus:{study_menu::Focus::start,study_menu::Focus::decorations,study_menu::Focus::badge}) {
   study_menu::Preferences prefs;
   if(!decoration)prefs.apply(study_menu::Action::toggle_decorations);
   if(badge)prefs.apply(study_menu::Action::next_badge);
   study_ui::Input click{{study_ui::Point{120,90}},{study_ui::Point{120,90}},true,false};
   CHECK(study_menu_render::controls(prefs,focus,click,labels,images,panel,rounded,text));
   CHECK(prefs.decorations()==bool(decoration)&&prefs.badge()==std::size_t(badge));
   CHECK(click.press.has_value());CHECK(gl.GetError()==0);++cases;
  }
  CHECK(atlas.clear());CHECK(!labels.centered(study_menu_render::Label::start,text,46,82));
 }
 std::printf("Widget rendering: %zu settings/focus/DPI cases, all seven labels, reset/revision guard; %s\n",cases,gl.GetString(Renderer));
 }
 SDL_GL_DeleteContext(context);SDL_DestroyWindow(window);SDL_Quit();
}
