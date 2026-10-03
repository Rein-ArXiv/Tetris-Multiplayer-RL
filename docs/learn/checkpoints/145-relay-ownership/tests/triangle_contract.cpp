#include "renderer/triangle.h"
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <string>
#include <climits>
#include <limits>
using namespace study_gl;
#define CHECK(x) do {if(!(x)){std::fprintf(stderr,"pass line %d\n",__LINE__);std::exit(1);}}while(0)
struct Fixture {int step=0,fail=0; GLenum error=0; GLuint program=0,vao=0; std::vector<std::string> ops;} f;
static bool tick(const char* name){f.ops.emplace_back(name);if(++f.step==f.fail){f.error=0x0502;return false;}return true;}
static GLenum STUDY_GL_CALL error(){auto e=f.error;f.error=0;return e;}
static void STUDY_GL_CALL use(GLuint n){if(tick(n?"program":"program0"))f.program=n;}
static void STUDY_GL_CALL bind(GLuint n){if(tick(n?"vao":"vao0"))f.vao=n;}
static void STUDY_GL_CALL viewport(GLint x,GLint y,GLsizei w,GLsizei h){CHECK(x==0&&y==0&&w==128&&h==128);tick("viewport");}
static void STUDY_GL_CALL color(GLfloat,GLfloat,GLfloat,GLfloat){tick("color");}
static void STUDY_GL_CALL clear(GLbitfield m){CHECK(m==ColorBufferBit);tick("clear");}
static void STUDY_GL_CALL draw(GLenum m,GLint first,GLsizei n){CHECK(m==Triangles&&n==6&&(first==0||first==6));CHECK(f.program==71&&f.vao==41);tick(first?"B":"A");}
int main(){
    GlApi gl;gl.GetError=error;gl.UseProgram=use;gl.BindVertexArray=bind;gl.Viewport=viewport;gl.ClearColor=color;gl.Clear=clear;gl.DrawArrays=draw;
    CHECK(begin_color_frame(gl,128,128,{0,0,0,1}));
    CHECK(submit_triangles(gl,71,41,12,0,6)&&submit_triangles(gl,71,41,12,6,6));
    CHECK(f.ops==std::vector<std::string>({"viewport","color","clear","program","vao","A","vao0","program0","program","vao","B","vao0","program0"}));
    for(int fail=1;fail<=3;++fail){f={};f.fail=fail;CHECK(!begin_color_frame(gl,128,128,{0,0,0,1})&&f.step==fail);}
    for(int fail=1;fail<=5;++fail){f={};f.fail=fail;CHECK(!submit_triangles(gl,71,41,12,0,6));CHECK(f.step==fail+2&&f.program==0&&f.vao==0);}
    f={};CHECK(!begin_color_frame(gl,0,128,{0,0,0,1})&&f.step==0);
    CHECK(!begin_color_frame(gl,128,128,{0,0,0,std::numeric_limits<double>::quiet_NaN()})&&f.step==0);
    const int invalid[][3]={{12,-1,6},{12,0,0},{12,0,4},{12,6,12},{12,13,3},{INT_MAX,INT_MAX-1,3},{-1,0,3}};
    for(const auto& a:invalid){f={};CHECK(!submit_triangles(gl,71,41,a[0],a[1],a[2])&&f.step==0);CHECK(!draw_triangles(gl,71,41,a[0],a[1],a[2],128,128)&&f.step==0);}
    f={};CHECK(!submit_triangles(gl,0,41,12,0,6)&&f.step==0);
    f.error=0x0502;CHECK(!submit_triangles(gl,71,41,12,0,6)&&f.step==0);
    f.error=0x0502;CHECK(!begin_color_frame(gl,128,128,{0,0,0,1})&&f.step==0);
    f={};CHECK(draw_triangles(gl,71,41,6,0,6,128,128));CHECK(f.ops.size()==8);
    std::puts("Pass: one clear/two draws, independent failures, cleanup, ranges and invalid entry passed");
}
