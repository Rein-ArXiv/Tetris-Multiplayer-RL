#include "renderer/triangle.h"
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
using namespace study_gl;
static void require(bool ok,const char* reason) {
    if(!ok) {std::fprintf(stderr,"triangle contract: %s\n",reason);std::exit(1);}
}
namespace {
struct Fixture {
    int step=0,fail=0;
    GLenum error=0;
    GLuint program=0,vao=0;
    std::vector<std::string> operations;
} f;
bool tick(const char* op) {
    f.operations.emplace_back(op);
    if (++f.step==f.fail) { f.error=0x0502;return false; }
    return true;
}
GLenum STUDY_GL_CALL error() {const GLenum e=f.error;f.error=0;return e;}
void STUDY_GL_CALL use(GLuint p) {require(p==71 || p==0,"program name");if(tick(p?"program":"program0"))f.program=p;}
void STUDY_GL_CALL bind(GLuint v) {require(v==41 || v==0,"VAO name");if(tick(v?"vao":"vao0"))f.vao=v;}
void STUDY_GL_CALL viewport(GLint x,GLint y,GLsizei w,GLsizei h) {
    require(x==0&&y==0&&w==1280&&h==960,"drawable pixels");tick("viewport");
}
void STUDY_GL_CALL color(GLfloat r,GLfloat g,GLfloat b,GLfloat a) {
    require(r==0.05f&&g==0.08f&&b==0.12f&&a==1.0f,"background");tick("color");
}
void STUDY_GL_CALL clear(GLbitfield mask) {require(mask==ColorBufferBit,"clear color attachment");tick("clear");}
void STUDY_GL_CALL draw(GLenum mode,GLint first,GLsizei count) {
    require(mode==Triangles&&first==0&&count==3,"draw vertex count, not byte count");
    require(f.program==71&&f.vao==41,"selected program and VAO");tick("draw");
}
}
int main() {
    GlApi gl;gl.GetError=error;gl.UseProgram=use;gl.BindVertexArray=bind;gl.Viewport=viewport;
    gl.ClearColor=color;gl.Clear=clear;gl.DrawArrays=draw;
    require(draw_triangle(gl,71,41,1280,960),"successful draw");
    require(f.operations==std::vector<std::string>{"program","vao","viewport","color","clear","draw","vao0","program0"},"frame order");
    require(f.program==0&&f.vao==0,"unbind success");
    // A failure must stop later work but still attempt both unbinds.
    for(int fail=1;fail<=8;++fail) {
        f={};f.fail=fail;
        require(!draw_triangle(gl,71,41,1280,960),"API failure detected");
        require(f.step==fail+2 && f.operations[f.operations.size()-2]=="vao0" && f.operations.back()=="program0","early return cleanup");
        require(f.program==0&&f.vao==0,"cleanup after injected failure");
    }
    for(auto dimensions : {std::pair<int,int>{0,960},{1280,0},{-1,960},{1280,-1}}) {
        f={};require(!draw_triangle(gl,71,41,dimensions.first,dimensions.second)&&f.step==0,"nonpositive rejected without GL calls");
    }
    for(auto names : {std::pair<GLuint,GLuint>{0,41},{71,0}}) {
        f={};require(!draw_triangle(gl,names.first,names.second,1280,960)&&f.step==0,"zero names rejected");
    }
    f={};f.error=0x0502;require(!draw_triangle(gl,71,41,1280,960)&&f.step==0,"dirty entry rejected");
    std::puts("Triangle contracts: frame order, vertex count, pixel dimensions, input rejection, eight failures and unbind passed");
}
