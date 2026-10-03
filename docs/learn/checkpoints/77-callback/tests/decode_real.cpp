#ifndef SDL_MAIN_HANDLED
#define SDL_MAIN_HANDLED
#endif
#include <SDL.h>
#include "renderer/texture_quad.h"
#include "renderer/image_decode.h"
#include "renderer/badge_pixels.h"
#include "renderer/flush_scene.h"
#include "renderer/flush_device.h"
#include "renderer/board_scene.h"
#include "client/game.h"
#include <vector>
#include <array>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#define CHECK(e) do{if(!(e)){std::fprintf(stderr,"CHECK failed line %d: %s SDL=%s\n",__LINE__,#e,SDL_GetError());std::exit(1);}}while(false)
using namespace study_gl;
int main(int argc,char** argv){
 CHECK(argc==2);
 SDL_SetMainReady();
 CHECK(SDL_Init(SDL_INIT_VIDEO)==0);
 CHECK(SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,3)==0&&SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,3)==0);
 CHECK(SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_CORE)==0);
 CHECK(SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS,SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG)==0);
 CHECK(SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER,0)==0&&SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS,0)==0);
 auto* window=SDL_CreateWindow("texture storage",0,0,640,480,SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN);CHECK(window);
 const auto context=SDL_GL_CreateContext(window);CHECK(context&&SDL_GL_MakeCurrent(window,context)==0);
 {
 GlApi gl;CHECK(load(gl,SDL_GL_GetProcAddress));
 // Poison unpack state and bind a real PBO. Upload must normalize then restore.
 GLuint other=0,pbo=0;gl.GenTextures(1,&other);gl.ActiveTexture(Texture0+2);gl.BindTexture(Texture2D,other);
 gl.GenBuffers(1,&pbo);gl.BindBuffer(PixelUnpackBuffer,pbo);gl.BufferData(PixelUnpackBuffer,512,nullptr,StaticDraw);
 gl.PixelStorei(UnpackAlignment,8);gl.PixelStorei(UnpackRowLength,19);gl.PixelStorei(UnpackSkipRows,2);gl.PixelStorei(UnpackSkipPixels,3);CHECK(gl.GetError()==0);
 study_texture::Texture odd(gl);std::array<unsigned char,24> bytes{};for(unsigned i=0;i<bytes.size();++i)bytes[i]=static_cast<unsigned char>(i*7);
 const auto saved=bytes;CHECK(odd.upload({bytes.data(),bytes.size(),3,2}));bytes.fill(0);
 GLint state=0;gl.GetIntegerv(TextureBinding2D,&state);CHECK(state==static_cast<GLint>(other));gl.GetIntegerv(ActiveTextureState,&state);CHECK(state==static_cast<GLint>(Texture0+2));
 for(const auto pair:{std::pair<GLenum,GLint>{UnpackAlignment,8},{UnpackRowLength,19},{UnpackSkipRows,2},{UnpackSkipPixels,3},{PixelUnpackBufferBinding,static_cast<GLint>(pbo)}}){gl.GetIntegerv(pair.first,&state);CHECK(state==pair.second);}
 gl.BindTexture(Texture2D,odd.name());std::array<unsigned char,24> read{};gl.GetTexImage(Texture2D,0,RGBA,UnsignedByte,read.data());CHECK(gl.GetError()==0&&read==saved);
 gl.GetTexLevelParameteriv(Texture2D,0,TextureWidth,&state);CHECK(state==3);gl.GetTexLevelParameteriv(Texture2D,0,TextureHeight,&state);CHECK(state==2);
 gl.BindTexture(Texture2D,other);
 study_texture::Texture badge(gl);{const auto decoded=study_image::decode_file(std::filesystem::u8path(argv[1]));CHECK(decoded&&badge.upload(decoded.image->view()));}
 gl.ActiveTexture(Texture0);gl.BindTexture(Texture2D,0);gl.BindBuffer(PixelUnpackBuffer,0);gl.DeleteBuffers(1,&pbo);gl.DeleteTextures(1,&other);
 CHECK(study_board_scene::configure(gl));study_batch::Device batch(gl);CHECK(batch.init());study_texture::Quad quad(gl);CHECK(quad.init());
 int w=0,h=0,dbl=0;SDL_GL_GetDrawableSize(window,&w,&h);CHECK(SDL_GL_GetAttribute(SDL_GL_DOUBLEBUFFER,&dbl)==0);gl.ReadBuffer(dbl?Back:Front);
 std::vector<unsigned char> before(std::size_t(w)*h*4),after(before.size());
 auto round=study_round::Round::create_seeded(study_grid::Grid{},1);CHECK(round);study_game::Game game(*round);
 for(study_letterbox::Size area:{study_letterbox::Size{640,480},{639,477},{479,479}}){
  const auto layout=study_letterbox::make_layout({640,480},area,{320,240});CHECK(layout);
  // Smaller drawable description intentionally tests viewport offsets in the same FBO.
  gl.Disable(ScissorTest);gl.ClearColor(0,0,0,1);gl.Clear(ColorBufferBit);
  CHECK(study_letterbox_scene::begin(gl,*layout,{.03,.06,.09,1}));
  auto view=game.view();CHECK(view);auto full=study_flush::full_clip(*layout);auto board=study_flush::project_clip(*layout,{110,20,100,200});CHECK(board);
  study_flush::GlSink sink{gl,batch};study_flush::Stream<study_flush::GlSink> stream(sink,full);
  CHECK(study_flush::scene(stream,*view,full,*board)&&stream.finish());gl.ReadPixels(0,0,w,h,RGBA,UnsignedByte,before.data());
  CHECK(sink.set_clip(full)&&quad.draw(badge));gl.ReadPixels(0,0,w,h,RGBA,UnsignedByte,after.data());CHECK(gl.GetError()==0);
  unsigned changed=0;
  for(int y=0;y<h;++y)for(int x=0;x<w;++x){
   const double lx=(x+.5-layout->viewport.x)*320./layout->viewport.width;
   const double ly=(area.height-(y+.5)-layout->viewport.y)*240./layout->viewport.height;
   const auto i=(std::size_t(y)*w+x)*4;
   if(lx>=20&&lx<68&&ly>=20&&ly<68){
    const auto col=static_cast<unsigned>((lx-20)*8/48),row=static_cast<unsigned>((ly-20)*8/48);
    const auto tex=(row*8+col)*4;
    for(unsigned c=0;c<4;++c)CHECK(after[i+c]==study_texture::badge_pixels[tex+c]);
    ++changed;
   }else for(unsigned c=0;c<4;++c)CHECK(after[i+c]==before[i+c]);
  }
  CHECK(changed>0);std::printf("%dx%d viewport: %u badge pixels exact, all other pixels preserved\n",area.width,area.height,changed);
 }
 std::printf("3x2 bytes copied despite hostile unpack state; GL renderer: %s\n",gl.GetString(Renderer));
 }
 SDL_GL_DeleteContext(context);SDL_DestroyWindow(window);SDL_Quit();
}
