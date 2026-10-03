#include "renderer/texture_quad.h"
#include <cstdio>
#include <cstdlib>
#include <set>
#include <vector>
#include <cstring>
using namespace study_gl;
#define CHECK(e) do{if(!(e)){std::fprintf(stderr,"CHECK failed line %d: %s\n",__LINE__,#e);std::exit(1);}}while(false)
struct Fake {
 GLuint next=1,buffer=0,array=0,program=0;GLenum error=0;
 int calls=0,draws=0,uploads=0,fail=0;bool zero_buffer=false,zero_array=false,attr_error=false;
 std::set<GLuint> buffers,arrays,programs,shaders;std::vector<unsigned char> data;
} f;
static void tick(){++f.calls;if(f.calls==f.fail)f.error=0x0502;}
static GLenum STUDY_GL_CALL error(){auto e=f.error;f.error=0;return e;}
static GLuint STUDY_GL_CALL shader(GLenum){auto n=f.next++;f.shaders.insert(n);return n;}
static void STUDY_GL_CALL source(GLuint,GLsizei,const GLchar*const*,const GLint*){}
static void STUDY_GL_CALL compile(GLuint){}
static void STUDY_GL_CALL shaderiv(GLuint,GLenum what,GLint* out){*out=what==CompileStatus?1:0;}
static void STUDY_GL_CALL delete_shader(GLuint n){CHECK(f.shaders.erase(n)==1);}
static GLuint STUDY_GL_CALL program(){auto n=f.next++;f.programs.insert(n);return n;}
static void STUDY_GL_CALL attach(GLuint,GLuint){}
static void STUDY_GL_CALL link(GLuint){}
static void STUDY_GL_CALL programiv(GLuint,GLenum what,GLint* out){*out=what==LinkStatus?1:0;}
static void STUDY_GL_CALL delete_program(GLuint n){CHECK(f.programs.erase(n)==1);}
static void STUDY_GL_CALL gen_buffer(GLsizei,GLuint* n){*n=f.zero_buffer?0:f.next++;if(*n)f.buffers.insert(*n);}
static void STUDY_GL_CALL gen_array(GLsizei,GLuint* n){*n=f.zero_array?0:f.next++;if(*n)f.arrays.insert(*n);}
static void STUDY_GL_CALL del_buffer(GLsizei,const GLuint* n){CHECK(f.buffers.erase(*n)==1);}
static void STUDY_GL_CALL del_array(GLsizei,const GLuint* n){CHECK(f.arrays.erase(*n)==1);}
static void STUDY_GL_CALL bind_buffer(GLenum target,GLuint n){CHECK(target==ArrayBuffer);tick();f.buffer=n;}
static void STUDY_GL_CALL bind_array(GLuint n){tick();f.array=n;}
static void STUDY_GL_CALL use(GLuint n){tick();f.program=n;}
static void STUDY_GL_CALL attribute(GLuint loc,GLint size,GLenum type,GLboolean normalized,GLsizei stride,const void* offset){
 CHECK(f.array&&f.buffer&&type==Float&&normalized==False&&stride==16);
 CHECK((loc==0&&size==2&&offset==nullptr)||(loc==1&&size==2&&reinterpret_cast<std::size_t>(offset)==8));
 if(f.attr_error)f.error=0x0502;
}
static void STUDY_GL_CALL enable(GLuint){}
static void STUDY_GL_CALL upload(GLenum target,GLsizeiptr bytes,const void* data,GLenum usage){
 CHECK(f.buffer&&target==ArrayBuffer&&usage==StaticDraw);tick();++f.uploads;
 const auto* p=static_cast<const unsigned char*>(data);f.data.assign(p,p+bytes);
}
static void STUDY_GL_CALL draw(GLenum mode,GLint first,GLsizei count){
 CHECK(mode==Triangles&&first==0&&f.program&&f.array&&static_cast<std::size_t>(count)*16==f.data.size());tick();++f.draws;
}
static bool uniform_missing=false;
static GLint STUDY_GL_CALL uniform_location(GLuint,const GLchar* name){CHECK(std::strcmp(name,"u_image")==0);return uniform_missing?-1:4;}
static GlApi api(){GlApi g;g.GetError=error;g.GetUniformLocation=uniform_location;g.CreateShader=shader;g.ShaderSource=source;g.CompileShader=compile;g.GetShaderiv=shaderiv;g.DeleteShader=delete_shader;g.CreateProgram=program;g.AttachShader=attach;g.DetachShader=attach;g.LinkProgram=link;g.GetProgramiv=programiv;g.DeleteProgram=delete_program;g.GenBuffers=gen_buffer;g.GenVertexArrays=gen_array;g.DeleteBuffers=del_buffer;g.DeleteVertexArrays=del_array;g.BindBuffer=bind_buffer;g.BindVertexArray=bind_array;g.UseProgram=use;g.VertexAttribPointer=attribute;g.EnableVertexAttribArray=enable;g.BufferData=upload;g.DrawArrays=draw;return g;}

int main(){auto gl=api();int calls=0;
 {study_texture::Quad quad(gl);CHECK(quad.init());calls=f.calls;CHECK(f.uploads==1&&f.data.size()==6*16&&f.array==0&&f.buffer==0);
  CHECK(!quad.init()&&f.calls==calls);study_texture::QuadVertex v[6];std::memcpy(v,f.data.data(),sizeof(v));
  CHECK(v[0].u==0&&v[0].v==0&&v[1].v==1&&v[2].u==1&&v[5].v==0);
 }
 CHECK(f.programs.empty()&&f.shaders.empty()&&f.buffers.empty()&&f.arrays.empty());
 for(int failure=1;failure<=calls;++failure){f={};f.fail=failure;
  {study_texture::Quad quad(gl);CHECK(!quad.init());if(failure<=2)CHECK(f.uploads==0);}
  CHECK(f.programs.empty()&&f.shaders.empty()&&f.buffers.empty()&&f.arrays.empty());
 }
 for(int kind=0;kind<4;++kind){f={};f.zero_buffer=kind==0;f.zero_array=kind==1;f.attr_error=kind==2;uniform_missing=kind==3;
  {study_texture::Quad quad(gl);CHECK(!quad.init());}
  CHECK(f.programs.empty()&&f.shaders.empty()&&f.buffers.empty()&&f.arrays.empty());
 }
 std::puts("Quad init: position/UV layout, bind failure prevents upload, failure cleanup and single ownership passed");
}
