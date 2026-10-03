// The checker inserts the actual production compile_shader function below.
#include "renderer/gl_api.h"
#include <cstdio>
#include <cstring>
#include <vector>
#include <cstdlib>
using namespace study_gl;
constexpr GLint GL_FALSE=0;
constexpr GLenum GL_COMPILE_STATUS=CompileStatus, GL_INFO_LOG_LENGTH=InfoLogLength;
static bool creation_ok,compilation_ok;
static unsigned source_calls,compile_calls,query_calls,deleted;
static void require(bool value) { if (!value) std::exit(1); }
static GLuint gl_CreateShader(GLenum) { return creation_ok ? 43 : 0; }
static void gl_ShaderSource(GLuint id,GLsizei,const GLchar* const*,const GLint*) { require(id==43); ++source_calls; }
static void gl_CompileShader(GLuint id) { require(id==43); ++compile_calls; }
static void gl_GetShaderiv(GLuint id,GLenum field,GLint* value) {
    require(id==43); ++query_calls; *value=field==GL_COMPILE_STATUS ? compilation_ok : 1;
}
static void gl_GetShaderInfoLog(GLuint id,GLsizei size,GLsizei*,GLchar* text) { require(id==43 && size>=1); text[0]=0; }
static void gl_DeleteShader(GLuint id) { require(id==43); ++deleted; }
// @PRODUCTION_COMPILE_SHADER@
int main() {
    require(compile_shader(VertexShader,"src","vertex")==0);
    require(source_calls==0 && compile_calls==0 && query_calls==0 && deleted==0);
    creation_ok=true;
    require(compile_shader(VertexShader,"src","vertex")==0 && deleted==1);
    compilation_ok=true;
    require(compile_shader(VertexShader,"src","vertex")==43 && deleted==1);
    gl_DeleteShader(43);
    require(source_calls==2 && compile_calls==2 && query_calls==3 && deleted==2);
    std::puts("Current renderer compile helper: creation failure short circuit and compile ownership passed");
}
