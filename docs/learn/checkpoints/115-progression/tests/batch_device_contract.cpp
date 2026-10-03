#include "renderer/batch_device.h"
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
 CHECK(f.array&&f.buffer&&type==Float&&normalized==False&&stride==24);
 CHECK((loc==0&&size==2&&offset==nullptr)||(loc==1&&size==4&&reinterpret_cast<std::size_t>(offset)==8));
 if(f.attr_error)f.error=0x0502;
}
static void STUDY_GL_CALL enable(GLuint){}
static void STUDY_GL_CALL upload(GLenum target,GLsizeiptr bytes,const void* data,GLenum usage){
 CHECK(f.buffer&&target==ArrayBuffer&&usage==DynamicDraw);tick();++f.uploads;
 const auto* p=static_cast<const unsigned char*>(data);f.data.assign(p,p+bytes);
}
static void STUDY_GL_CALL draw(GLenum mode,GLint first,GLsizei count){
 CHECK(mode==Triangles&&first==0&&f.program&&f.array&&static_cast<std::size_t>(count)*24==f.data.size());tick();++f.draws;
}
static GlApi api(){GlApi g;g.GetError=error;g.CreateShader=shader;g.ShaderSource=source;g.CompileShader=compile;g.GetShaderiv=shaderiv;g.DeleteShader=delete_shader;g.CreateProgram=program;g.AttachShader=attach;g.DetachShader=attach;g.LinkProgram=link;g.GetProgramiv=programiv;g.DeleteProgram=delete_program;g.GenBuffers=gen_buffer;g.GenVertexArrays=gen_array;g.DeleteBuffers=del_buffer;g.DeleteVertexArrays=del_array;g.BindBuffer=bind_buffer;g.BindVertexArray=bind_array;g.UseProgram=use;g.VertexAttribPointer=attribute;g.EnableVertexAttribArray=enable;g.BufferData=upload;g.DrawArrays=draw;return g;}
int main(){auto gl=api();const study_mesh::Vertex2 tri[]={{0,0},{1,0},{0,1}};study_batch::Batch b;CHECK(b.append(tri,3,{1,0,0,1})&&b.append(tri,3,{0,1,0,1}));
 {
 study_batch::Device d(gl);CHECK(!d.submit(b));CHECK(d.init());CHECK(!d.init());
 CHECK(d.submit(b)&&f.draws==1&&f.uploads==1);CHECK(f.data.size()==6*24&&std::memcmp(f.data.data(),b.data(),f.data.size())==0);
 CHECK(f.program==0&&f.array==0&&f.buffer==0);
 const auto calls=f.calls;b.clear();CHECK(d.submit(b)&&f.calls==calls);
 CHECK(b.append(tri,3,{0,0,1,1}));CHECK(d.submit(b)&&f.data.size()==3*24);
 for(int failure=1;failure<=8;++failure){f.calls=0;f.fail=failure;f.draws=0;f.uploads=0;CHECK(!d.submit(b));if(failure==1)CHECK(f.uploads==0);CHECK(f.program==0&&f.array==0&&f.buffer==0);}
 f.fail=0;f.calls=0;f.error=0x0502;CHECK(!d.submit(b)&&f.calls==0);
 }
 CHECK(f.programs.empty()&&f.shaders.empty()&&f.buffers.empty()&&f.arrays.empty());
 for(int failure=0;failure<3;++failure){f={};f.zero_buffer=failure==0;f.zero_array=failure==1;f.attr_error=failure==2;{study_batch::Device d(gl);CHECK(!d.init());}CHECK(f.programs.empty()&&f.shaders.empty()&&f.buffers.empty()&&f.arrays.empty());}
 std::puts("Batch GPU contract: stride/offset, used upload, one draw, empty no-op, errors and cleanup passed");
}
