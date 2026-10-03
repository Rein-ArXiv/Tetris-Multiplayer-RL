// Insert the real renderer_begin to verify stale viewport removal, not a copy.
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <array>
using GLuint=unsigned;using GLenum=unsigned;using GLint=int;
constexpr GLenum GL_SCISSOR_TEST=0x0C11,GL_COLOR_BUFFER_BIT=0x4000;
struct Color { unsigned char r,g,b,a; };
static bool s_ready=true;
static GLuint s_prog=71,s_white=42,s_batch_tex=0;
static int s_screen_h=480,s_screen_w=640,s_u_screen=1,s_u_tex=2;
static float s_render_scale=1;
static std::vector<float> s_verts;
static int width=640,height=480,viewport_calls=0,clears=0,uses=0;
static std::array<int,4> viewport{5,7,640,480};
static void require(bool ok,const char* why){if(!ok){std::fprintf(stderr,"frame contract: %s\n",why);std::exit(1);}}
static void platform_viewport(int& x,int& y,int& w,int& h){x=5;y=7;w=width;h=height;}
static void gl_Viewport(int x,int y,int w,int h){viewport={x,y,w,h};++viewport_calls;}
static void gl_UseProgram(GLuint p){require(p==s_prog,"program");++uses;}
static void gl_Disable(GLenum e){require(e==GL_SCISSOR_TEST,"disable");}
static void gl_Enable(GLenum e){require(e==GL_SCISSOR_TEST,"enable");}
static void gl_Scissor(int x,int y,int w,int h){require(x==5&&y==7&&w==640&&h==480,"scissor");}
static void gl_ClearColor(float,float,float,float){}
static void gl_Clear(GLenum e){require(e==GL_COLOR_BUFFER_BIT,"clear mask");++clears;}
static void gl_Uniform2f(GLint,float,float){}
static void gl_Uniform1i(GLint,int){}
// @PRODUCTION_RENDERER_BEGIN@
int main(){
    for(auto size:{std::pair<int,int>{0,480},{640,0},{0,0},{-1,480}}){
        width=size.first;height=size.second;viewport_calls=clears=uses=0;
        viewport={5,7,640,480};s_verts={1,2};s_batch_tex=0;
        renderer_begin({1,2,3,255});
        require(viewport_calls==1&&viewport==std::array<int,4>{0,0,0,0},"empty viewport must replace stale GL viewport");
        require(clears==0&&uses==1&&s_verts.empty()&&s_batch_tex==s_white,"empty pass batch contract");
    }
    width=640;height=480;viewport_calls=clears=uses=0;
    renderer_begin({1,2,3,255});
    require(viewport==std::array<int,4>{5,7,640,480}&&viewport_calls==1&&clears==2&&uses==1,"restored positive pass");
    s_ready=false;viewport_calls=clears=uses=0;renderer_begin({1,2,3,255});
    require(viewport_calls==0&&clears==0&&uses==0,"uninitialized no-op");
    std::puts("Current renderer_begin: empty viewport removes stale GL state and positive viewport resumes");
}
