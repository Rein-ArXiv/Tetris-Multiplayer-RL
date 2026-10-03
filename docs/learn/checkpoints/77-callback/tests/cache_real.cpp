#ifndef SDL_MAIN_HANDLED
#define SDL_MAIN_HANDLED
#endif
#include <SDL.h>
#include "renderer/cached_text.h"
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
 SDL_SetMainReady();
 CHECK(argc==2&&SDL_Init(SDL_INIT_VIDEO)==0);
 CHECK(SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,3)==0&&SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,3)==0);
 CHECK(SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_CORE)==0);
 CHECK(SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS,SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG)==0);
 CHECK(SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER,0)==0&&SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS,0)==0);
 auto* window=SDL_CreateWindow("glyph",0,0,1280,960,SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN);CHECK(window);
 auto context=SDL_GL_CreateContext(window);CHECK(context&&SDL_GL_MakeCurrent(window,context)==0);
 {
 GlApi gl;CHECK(load(gl,SDL_GL_GetProcAddress)&&study_board_scene::configure(gl));
 study_font::Font font;CHECK(font.load_trusted(std::filesystem::u8path(argv[1])));
 study_font::GlyphCache cpu;CHECK(cpu.load_trusted(std::filesystem::u8path(argv[1])));
 study_image_quad::ImageQuad quad(gl);CHECK(quad.init(study_image_quad::Sampling::coverage));
 study_atlas::GlyphAtlas atlas(gl,512,512);CHECK(atlas.init());
 study_text::GpuGlyphCache cache(atlas);
 int drawable_w=0,drawable_h=0,dbl=0;SDL_GL_GetDrawableSize(window,&drawable_w,&drawable_h);
 CHECK(drawable_w>=1080&&drawable_h>=810&&SDL_GL_GetAttribute(SDL_GL_DOUBLEBUFFER,&dbl)==0);gl.ReadBuffer(dbl?Back:Front);
 std::size_t checked=0,skipped=0,ink=0,cases=0;
 for(const auto text:{u8"대기실",u8"플레이",u8"AV\nA V\n"}) for(int height:{16,22}) for(double density:{.55,1.,1.25,2.,3.375}) {
  // Render a smaller drawable rectangle in the existing offscreen buffer. The
  // physical viewport varies while logical units and baseline remain fixed.
  const int w=int(320*density),h=int(240*density);
  std::vector<unsigned char> before(std::size_t(w)*h*4),after(before.size());
  const auto label=study_labels::decode(text);CHECK(label);
  const auto logical=study_font::prepare_paragraph(font,*label,float(height));CHECK(logical);
  const auto line=study_font::prepare_cached_paragraph(cpu,*label,height,density);CHECK(line);
  const int device_height=int(std::round(height*std::max(1.,std::round(density*8)/8)));
  const double ratio=double(height)/device_height;
  std::vector<study_font::Glyph> expected_glyphs;
  for(std::size_t i=0;i<label->count;++i) if(label->scalars[i]!=U'\n') {
    const auto glyph=font.rasterize_device(label->scalars[i],device_height);CHECK(glyph);
    expected_glyphs.push_back(*glyph);
  }
  CHECK(atlas.clear());
  {
  study_text::CachedText gpu;CHECK(gpu.init(cache,*line));
  const auto layout=study_letterbox::make_layout({w,h},{w,h},{320,240});CHECK(layout);
  CHECK(study_letterbox_scene::begin(gl,*layout,{.125,.25,.375,1}));
  gl.ReadPixels(0,0,w,h,RGBA,UnsignedByte,before.data());CHECK(gl.GetError()==0);
  constexpr float x_start=20.25f,baseline=82.125f;
  CHECK(gpu.draw(quad,x_start,baseline));gl.ReadPixels(0,0,w,h,RGBA,UnsignedByte,after.data());CHECK(gl.GetError()==0);
  for(int y=0;y<h;++y)for(int x=0;x<w;++x) {
   const auto at=(std::size_t(y)*w+x)*4;
   const double lx=(x+.5-layout->viewport.x)*320./layout->viewport.width;
   const double ly=(h-y-.5-layout->viewport.y)*240./layout->viewport.height;
   std::array<double,4> expected{double(before[at]),double(before[at+1]),double(before[at+2]),double(before[at+3])};
   bool exclude=false,painted=false;
   for(std::size_t n=0;n<logical->count;++n) {
    const auto& p=logical->items[n];const auto& g=expected_glyphs[n];
    const double dx=(lx-(x_start+p.pen_x))/ratio-g.xoff,dy=(ly-(baseline+p.baseline))/ratio-g.yoff;
    if(dx>=0&&dy>=0&&dx<g.width&&dy<g.height) {
     if(std::abs(dx-std::round(dx))<.0001||std::abs(dy-std::round(dy))<.0001){exclude=true;break;}
     const double sx=dx-.5,sy=dy-.5;const int ix=int(std::floor(sx)),iy=int(std::floor(sy));
     const double fx=sx-ix,fy=sy-iy;
     const auto texel=[&](int col,int row){return col<0||row<0||col>=g.width||row>=g.height ? 0. : double(g.coverage[std::size_t(row)*g.width+col])/255.;};
     const double alpha=(1-fy)*((1-fx)*texel(ix,iy)+fx*texel(ix+1,iy))+fy*((1-fx)*texel(ix,iy+1)+fx*texel(ix+1,iy+1));
     for(int c=0;c<3;++c)expected[c]=std::round(255*alpha+expected[c]*(1-alpha));
     painted|=alpha>0;
    }
   }
   if(exclude){++skipped;continue;}
   for(int c=0;c<4;++c)CHECK(std::abs(after[at+c]-expected[c])<=2.01);
   ++checked;ink+=painted;
  }
  CHECK(atlas.clear());CHECK(!gpu.draw(quad,x_start,baseline));
  gpu.reset();CHECK(gpu.init(cache,*line));
  ++cases;cpu.clear();
  }

 }
 CHECK(ink>1000);std::printf("Cached text DPI CPU/GPU: %zu cases, %zu pixels, %zu ink, %zu boundary exclusions; %s\n",cases,checked,ink,skipped,gl.GetString(Renderer));
 }
 SDL_GL_DeleteContext(context);SDL_DestroyWindow(window);SDL_Quit();
}
