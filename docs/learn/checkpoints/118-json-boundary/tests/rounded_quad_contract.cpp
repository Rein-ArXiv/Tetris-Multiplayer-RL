#include "renderer/rounded_quad.h"
#include <cstdio>
#include <cstdlib>
#include <set>
#include <vector>
#include <cstring>
using namespace study_gl;
#define CHECK(e) do{if(!(e)){std::fprintf(stderr,"CHECK failed line %d: %s\n",__LINE__,#e);std::exit(1);}}while(false)
struct Fake {
 GLuint next=1,buffer=0,array=0,program=0;GLenum error=0;
 int calls=0,draws=0,uploads=0,fail=0;bool zero_buffer=false,zero_array=false,attr_error=false,blend=false; GLuint texture=0;bool texture_live=false;
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
 CHECK(f.array&&f.buffer&&type==Float&&normalized==False&&stride==52);
 CHECK((loc==0&&size==2&&offset==nullptr)||(loc==1&&size==2&&reinterpret_cast<std::size_t>(offset)==8)||(loc==2&&size==4&&reinterpret_cast<std::size_t>(offset)==16)||(loc==3&&size==2&&reinterpret_cast<std::size_t>(offset)==32)||(loc==4&&size==2&&reinterpret_cast<std::size_t>(offset)==40)||(loc==5&&size==1&&reinterpret_cast<std::size_t>(offset)==48));
 if(f.attr_error)f.error=0x0502;
}
static void STUDY_GL_CALL enable(GLuint){}
static void STUDY_GL_CALL upload(GLenum target,GLsizeiptr bytes,const void* data,GLenum usage){
 CHECK(f.buffer&&target==ArrayBuffer&&usage==DynamicDraw);tick();++f.uploads;
 const auto* p=static_cast<const unsigned char*>(data);if(p)f.data.assign(p,p+bytes);else f.data.assign(static_cast<std::size_t>(bytes),0);
}
static void STUDY_GL_CALL draw(GLenum mode,GLint first,GLsizei count){
 CHECK(mode==Triangles&&first==0&&f.program&&f.array&&static_cast<std::size_t>(count)*52==f.data.size());tick();++f.draws;
}
static bool uniform_missing=false;
static GLint STUDY_GL_CALL uniform_location(GLuint,const GLchar* name){CHECK(std::strcmp(name,"u_image")==0);return uniform_missing?-1:4;}
static GlApi api(){GlApi g;g.GetError=error;g.GetUniformLocation=uniform_location;g.CreateShader=shader;g.ShaderSource=source;g.CompileShader=compile;g.GetShaderiv=shaderiv;g.DeleteShader=delete_shader;g.CreateProgram=program;g.AttachShader=attach;g.DetachShader=attach;g.LinkProgram=link;g.GetProgramiv=programiv;g.DeleteProgram=delete_program;g.GenBuffers=gen_buffer;g.GenVertexArrays=gen_array;g.DeleteBuffers=del_buffer;g.DeleteVertexArrays=del_array;g.BindBuffer=bind_buffer;g.BindVertexArray=bind_array;g.UseProgram=use;g.VertexAttribPointer=attribute;g.EnableVertexAttribArray=enable;g.BufferData=upload;g.DrawArrays=draw;return g;}


static void STUDY_GL_CALL active(GLenum u){CHECK(u==Texture0);tick();}
static void STUDY_GL_CALL bind_texture(GLenum,GLuint n){f.texture=n;tick();}
static void STUDY_GL_CALL get_int(GLenum what,GLint* out){*out=what==MaxTextureSize?1024:what==TextureBinding2D?int(f.texture):what==UnpackAlignment?4:0;}
static void STUDY_GL_CALL gen_texture(GLsizei,GLuint* n){CHECK(!f.texture_live);*n=300;f.texture_live=true;}
static void STUDY_GL_CALL del_texture(GLsizei,const GLuint* n){CHECK(*n==300&&f.texture_live);f.texture_live=false;}
static void STUDY_GL_CALL parameter(GLenum,GLenum,GLint){}
static void STUDY_GL_CALL pixel(GLenum,GLint){}
static void STUDY_GL_CALL pixels(GLenum,GLint,GLint,GLsizei,GLsizei,GLint,GLenum,GLenum,const void*){}
static void STUDY_GL_CALL image_buffer(GLenum target,GLuint n){if(target==ArrayBuffer)bind_buffer(target,n);else CHECK(target==PixelUnpackBuffer);}
static void STUDY_GL_CALL uniform(GLint loc,GLint unit){CHECK(loc==4&&unit==0);tick();}
static void STUDY_GL_CALL equation(GLenum eq){CHECK(eq==FuncAdd);tick();}
static void STUDY_GL_CALL factors(GLenum a,GLenum b,GLenum c,GLenum d){CHECK(a==SrcAlpha&&b==OneMinusSrcAlpha&&c==One&&d==OneMinusSrcAlpha);tick();}
static void STUDY_GL_CALL enable_blend(GLenum cap){CHECK(cap==Blend);f.blend=true;tick();}
static void STUDY_GL_CALL disable_blend(GLenum cap){CHECK(cap==Blend);f.blend=false;tick();}
static GlApi full_api(){auto g=api();g.ActiveTexture=active;g.BindTexture=bind_texture;g.GetIntegerv=get_int;g.GenTextures=gen_texture;g.DeleteTextures=del_texture;g.TexParameteri=parameter;g.PixelStorei=pixel;g.TexImage2D=pixels;g.BindBuffer=image_buffer;g.Uniform1i=uniform;g.BlendEquation=equation;g.BlendFuncSeparate=factors;g.Enable=enable_blend;g.Disable=disable_blend;return g;}
int main(){auto gl=full_api();int init_calls=0;
 {study_rounded::RoundedQuad q(gl);CHECK(q.init());init_calls=f.calls;CHECK(f.uploads==1&&f.data.size()==312&&!f.array&&!f.buffer);}
 for(int failure=1;failure<=init_calls;++failure){f={};f.fail=failure;{study_rounded::RoundedQuad q(gl);CHECK(!q.init());if(failure<=2)CHECK(f.uploads==0);}CHECK(f.buffers.empty()&&f.arrays.empty()&&f.programs.empty()&&f.shaders.empty());}
 for(int kind=0;kind<4;++kind){f={};f.zero_buffer=kind==0;f.zero_array=kind==1;f.attr_error=kind==2;uniform_missing=kind==3;
  {study_rounded::RoundedQuad q(gl);CHECK(!q.init());}
  CHECK(f.buffers.empty()&&f.arrays.empty()&&f.programs.empty()&&f.shaders.empty());
 }
 uniform_missing=false;
 int draw_calls=0;
 {f={};study_texture::Texture tex(gl);unsigned char data[4]={255,0,0,128};CHECK(tex.upload({data,4,1,1}));study_rounded::RoundedQuad q(gl);CHECK(q.init());
  study_rounded::Draw d;d.radius=12;d.image.uv={1,0,0,1};d.image.tint={.5f,1,.25f,.5f};int start=f.calls;CHECK(q.draw(tex,d));draw_calls=f.calls-start;
  CHECK(f.draws==1&&!f.blend&&!f.array&&!f.buffer&&!f.program&&!f.texture);
  study_rounded::Vertex v[6];std::memcpy(v,f.data.data(),sizeof(v));CHECK(v[0].u==1&&v[5].u==0&&v[0].a==.5f&&v[2].r==.5f&&v[0].local_x==-24&&v[0].radius==12);
  start=f.calls;d.radius=25;CHECK(!q.draw(tex,d)&&f.calls==start);d.radius=12;
  start=f.calls;d.image.angle_degrees=std::numeric_limits<float>::infinity();CHECK(!q.draw(tex,d)&&f.calls==start&&f.draws==1);
 }
 for(int failure=1;failure<=draw_calls;++failure){f={};{study_texture::Texture tex(gl);unsigned char data[4]{};CHECK(tex.upload({data,4,1,1}));study_rounded::RoundedQuad q(gl);CHECK(q.init());
  const auto uploads=f.uploads;f.fail=f.calls+failure;CHECK(!q.draw(tex,{}));if(failure<=2)CHECK(f.uploads==uploads);if(failure<=10)CHECK(f.draws==0);
  f.fail=0;CHECK(q.draw(tex,{})&&!f.blend&&!f.array&&!f.buffer&&!f.program&&!f.texture);
 }CHECK(!f.texture_live&&f.buffers.empty()&&f.arrays.empty()&&f.programs.empty()&&f.shaders.empty());}
 std::printf("RoundedQuad: layout52/local32/half40/radius48, %d init and %d draw failures, validation before GL, stale-upload suppression and retry passed\n",init_calls,draw_calls);
}
