#ifndef SDL_MAIN_HANDLED
#define SDL_MAIN_HANDLED
#endif
#include <SDL.h>
#include "renderer/texture_quad.h"
#include "renderer/image_store.h"
#include "renderer/session_icons.h"
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
 (void)argc;(void)argv;
 SDL_SetMainReady();
 CHECK(SDL_Init(SDL_INIT_VIDEO)==0);
 CHECK(SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,3)==0&&SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,3)==0);
 CHECK(SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_CORE)==0);
 CHECK(SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS,SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG)==0);
 CHECK(SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE,8)==0&&SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER,0)==0&&SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS,0)==0);
 auto* window=SDL_CreateWindow("texture storage",0,0,640,480,SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN);CHECK(window);
 const auto context=SDL_GL_CreateContext(window);CHECK(context&&SDL_GL_MakeCurrent(window,context)==0);
 {
 GlApi gl;CHECK(load(gl,SDL_GL_GetProcAddress));
 CHECK(study_board_scene::configure(gl));study_batch::Device batch(gl);CHECK(batch.init());
 study_image::ImageStore images(gl);study_rounded::RoundedQuad quad(gl);CHECK(quad.init());
 const unsigned char pixels[]={255,0,0,128, 0,255,0,255, 0,0,255,0, 200,100,50,64};
 const auto handle=images.create({pixels,16,2,2});CHECK(handle);
 int w=0,h=0,dbl=0;SDL_GL_GetDrawableSize(window,&w,&h);CHECK(SDL_GL_GetAttribute(SDL_GL_DOUBLEBUFFER,&dbl)==0);gl.ReadBuffer(dbl?Back:Front);
 std::vector<unsigned char> before(std::size_t(w)*h*4),after(before.size());
 auto round=study_round::Round::create_seeded(study_grid::Grid{},1);CHECK(round);study_game::Game game(*round);
 std::array<study_rounded::Draw,8> cases{};
 cases[1].image.rect={20,80,64,32};cases[1].image.uv={.25f,0,.75f,.5f};cases[1].image.tint={.5f,1,.25f,1};
 cases[2].image.uv={1,0,0,1};
 cases[3].image.rect={80,50,64,32};cases[3].image.angle_degrees=90;
 cases[4].image.rect={100,80,48,32};cases[4].image.pivot_x=cases[4].image.pivot_y=0;cases[4].image.angle_degrees=-90;
 cases[5].image.rect={42,50,48,36};cases[5].image.angle_degrees=27;cases[5].image.tint={.8f,.5f,1,.5f};
 cases[6].image.rect={-10,20,48,48};cases[6].image.angle_degrees=-12;
 cases[7].image.uv={.25f,.25f,.25f,.25f};cases[7].image.tint={1,.5f,.5f,.25f};
 const float radii[8]={0,8,24,16,12,14,20,.5f};
 for(int i=0;i<8;++i)cases[i].radius=radii[i];
 std::size_t checked_total=0,excluded_total=0,soft_total=0;
 for(const auto& request_draw:cases)for(study_letterbox::Size area:{study_letterbox::Size{640,480},{639,477},{479,479}}){
  const auto& request=request_draw.image;
  const auto layout=study_letterbox::make_layout({640,480},area,{320,240});CHECK(layout);
  gl.Disable(ScissorTest);gl.ClearColor(0,0,0,1);gl.Clear(ColorBufferBit);
  // Premultiplied destination: straight RGB (.1,.2,.3), alpha .25.
  CHECK(study_letterbox_scene::begin(gl,*layout,{.025,.05,.075,.25}));
  auto view=game.view();CHECK(view);auto full=study_flush::full_clip(*layout);auto board=study_flush::project_clip(*layout,{110,20,100,200});CHECK(board);
  study_flush::GlSink sink{gl,batch};study_flush::Stream<study_flush::GlSink> stream(sink,full);
  CHECK(study_flush::scene(stream,*view,full,*board)&&stream.finish());gl.ReadPixels(0,0,w,h,RGBA,UnsignedByte,before.data());
  CHECK(sink.set_clip(full)&&images.draw(quad,handle,request_draw));gl.ReadPixels(0,0,w,h,RGBA,UnsignedByte,after.data());CHECK(gl.GetError()==0);
  const auto r=request.rect;const double theta=request.angle_degrees*3.14159265358979323846/180;
  const double c=std::cos(theta),sn=std::sin(theta),cx=r.x+request.pivot_x*r.width,cy=r.y+request.pivot_y*r.height;
  unsigned inside=0,excluded=0,alpha_destination=0;
  for(int y=0;y<h;++y)for(int x=0;x<w;++x){
   const double lx=(x+.5-layout->viewport.x)*320./layout->viewport.width;
   const double ly=(area.height-(y+.5)-layout->viewport.y)*240./layout->viewport.height;
   const auto i=(std::size_t(y)*w+x)*4;
   const double dx=lx-cx,dy=ly-cy;
   const double localx=c*dx+sn*dy+request.pivot_x*r.width;
   const double localy=-sn*dx+c*dy+request.pivot_y*r.height;
   // Scalar inverse transform is independent of the CPU vertex expansion.
   const bool clipped=lx<0||lx>=320||ly<0||ly>=240;
   if(!clipped && (std::abs(localx)<.02||std::abs(localx-r.width)<.02||std::abs(localy)<.02||std::abs(localy-r.height)<.02)){++excluded;continue;}
   const bool covered=!clipped&&localx>=0&&localx<r.width&&localy>=0&&localy<r.height;
   if(covered){
    const double u=request.uv.u0+(request.uv.u1-request.uv.u0)*localx/r.width;
    const double v=request.uv.v0+(request.uv.v1-request.uv.v0)*localy/r.height;
    if((request.uv.u0!=request.uv.u1&&std::abs(u*2-std::round(u*2))<.0001)||
       (request.uv.v0!=request.uv.v1&&std::abs(v*2-std::round(v*2))<.0001)){++excluded;continue;}
    const int col=std::clamp(int(std::floor(u*2)),0,1),row=std::clamp(int(std::floor(v*2)),0,1);const auto tex=(row*2+col)*4;
    const double tint[]={request.tint.r,request.tint.g,request.tint.b,request.tint.a};
    double mask=1;
    if(request_draw.radius>0){
     // Piecewise distance to the rounded boundary (independent of GLSL q formula).
     const double ax=std::abs(localx-r.width*.5),ay=std::abs(localy-r.height*.5);
     const double cx=r.width*.5-request_draw.radius,cy=r.height*.5-request_draw.radius;
     double distance;
     if(ax<=cx)distance=ay-r.height*.5;
     else if(ay<=cy)distance=ax-r.width*.5;
     else distance=std::hypot(ax-cx,ay-cy)-request_draw.radius;
     // When both coordinates are in the core, the nearer straight edge wins.
     if(ax<=cx&&ay<=cy)distance=(std::max)(ax-r.width*.5,ay-r.height*.5);
     const double t=(std::clamp)(distance+.5,0.,1.);mask=1-3*t*t+2*t*t*t;
    }
    if(mask>0&&mask<1)++soft_total;
    const double alpha=pixels[tex+3]/255.*tint[3]*mask;
    for(int k=0;k<4;++k){double expected=k==3?255*alpha+before[i+k]*(1-alpha):pixels[tex+k]*tint[k]*alpha+before[i+k]*(1-alpha);
     if(std::abs(double(after[i+k])-expected)>2.01){std::fprintf(stderr,"pixel mismatch x%d y%d ch%d got%d expected%f angle%f\n",x,y,k,after[i+k],expected,request.angle_degrees);std::exit(1);}}
    ++inside;if(before[i+3]<250)++alpha_destination;
   }else for(int k=0;k<4;++k)CHECK(after[i+k]==before[i+k]);
   ++checked_total;
  }
  CHECK(inside>250);CHECK(alpha_destination>100);excluded_total+=excluded;
  std::printf("angle %.1f viewport %dx%d: %u interior samples, %u boundary exclusions; exterior preserved\n",request.angle_degrees,area.width,area.height,inside,excluded);
 }
 study_rounded::Draw invalid;invalid.radius=std::numeric_limits<float>::infinity();
 gl.ReadPixels(0,0,w,h,RGBA,UnsignedByte,before.data());CHECK(!images.draw(quad,handle,invalid));
 CHECK(images.unload(handle)&&!images.draw(quad,handle,{}));gl.ReadPixels(0,0,w,h,RGBA,UnsignedByte,after.data());CHECK(before==after&&gl.GetError()==0);
 CHECK(soft_total>100);std::printf("%zu smooth mask samples\n",soft_total);
 std::printf("24 rounded cases: %zu checked pixels, %zu boundary exclusions; GL renderer %s\n",checked_total,excluded_total,gl.GetString(Renderer));
 }
 SDL_GL_DeleteContext(context);SDL_DestroyWindow(window);SDL_Quit();
}
