// The checker inserts the actual production link_program, not a copied helper.
#include "renderer/gl_api.h"
#include <cstdio>
#include <cstring>
#include <vector>
#include <cstdlib>
using namespace study_gl;
constexpr GLint GL_FALSE=0;
constexpr GLenum GL_VERTEX_SHADER=VertexShader, GL_FRAGMENT_SHADER=FragmentShader;
constexpr GLenum GL_LINK_STATUS=LinkStatus, GL_INFO_LOG_LENGTH=InfoLogLength;
static bool vertex_ok=true,fragment_ok=true,creation_ok=false,link_ok=false;
static unsigned created,attached,linked,queried,deleted_program,deleted_vertex,deleted_fragment;
static void require(bool v) { if(!v) std::exit(1); }
static GLuint compile_shader(GLenum type,const char*,const char*) {
    return type==VertexShader ? (vertex_ok?11:0) : (fragment_ok?12:0);
}
static GLuint gl_CreateProgram() { ++created; return creation_ok?71:0; }
static void gl_AttachShader(GLuint p,GLuint s) { require(p==71 && (s==11 || s==12)); ++attached; }
static void gl_LinkProgram(GLuint p) { require(p==71); ++linked; }
static void gl_GetProgramiv(GLuint p,GLenum what,GLint* value) {
    require(p==71); ++queried; *value=what==LinkStatus ? link_ok : 1;
}
static void gl_GetProgramInfoLog(GLuint p,GLsizei cap,GLsizei*,GLchar* out) { require(p==71 && cap>=1); out[0]=0; }
static void gl_DeleteProgram(GLuint p) { require(p==71); ++deleted_program; }
static void gl_DeleteShader(GLuint s) { require(s==11 || s==12); if(s==11) ++deleted_vertex; else ++deleted_fragment; }
// @PRODUCTION_LINK_PROGRAM@
int main() {
    require(link_program("v","f")==0 && created==1 && attached==0 && linked==0 && queried==0);
    require(deleted_vertex==1 && deleted_fragment==1 && deleted_program==0);
    creation_ok=true;
    require(link_program("v","f")==0 && attached==2 && linked==1 && queried==2);
    require(deleted_program==1 && deleted_vertex==2 && deleted_fragment==2);
    link_ok=true;
    require(link_program("v","f")==71 && deleted_program==1 && deleted_vertex==3 && deleted_fragment==3);
    gl_DeleteProgram(71);
    vertex_ok=false;
    require(link_program("v","f")==0 && created==3 && deleted_vertex==3 && deleted_fragment==4);
    fragment_ok=false; vertex_ok=true;
    require(link_program("v","f")==0 && created==3 && deleted_vertex==4 && deleted_fragment==4);
    std::puts("Production link helper: zero creation short circuit, shader failure, link failure and success ownership passed");
}
