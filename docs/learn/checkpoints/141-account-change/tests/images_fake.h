#pragma once
#include "renderer/gl_api.h"
#include <map>
#include <set>
#include <vector>
#include <cstdio>
#include <cstdlib>
#include <cstring>
using namespace study_gl;
#define CHECK(e) do{if(!(e)){std::fprintf(stderr,"CHECK failed line %d: %s\n",__LINE__,#e);std::exit(1);}}while(false)
struct FakeTexture {
 GLenum error=0;int call=0,fail=0,uploads=0,gens=0,deletes=0;bool zero=false,upload_error=false;
 std::map<GLenum,GLint> state{{MaxTextureSize,1024},{TextureBinding2D,77},{UnpackAlignment,8},
 {UnpackRowLength,9},{UnpackSkipRows,2},{UnpackSkipPixels,3},{PixelUnpackBufferBinding,55}};
 std::set<GLuint> live;std::vector<unsigned char> copy;
} f;
static bool tick(){++f.call;if(f.call==f.fail){f.error=0x0502;return false;}return true;}
static GLenum STUDY_GL_CALL get_error(){auto e=f.error;f.error=0;return e;}
static void STUDY_GL_CALL get_int(GLenum what,GLint* out){if(tick())*out=f.state.at(what);}
static void STUDY_GL_CALL gen(GLsizei n,GLuint* out){CHECK(n==1);++f.gens;if(!tick()||f.zero){*out=0;return;}*out=static_cast<GLuint>(f.gens);CHECK(f.live.insert(*out).second);}
static void STUDY_GL_CALL bind(GLenum target,GLuint n){CHECK(target==Texture2D);if(tick())f.state[TextureBinding2D]=static_cast<GLint>(n);}
static void STUDY_GL_CALL buffer(GLenum target,GLuint n){CHECK(target==PixelUnpackBuffer);if(tick())f.state[PixelUnpackBufferBinding]=static_cast<GLint>(n);}
static void STUDY_GL_CALL store(GLenum what,GLint value){if(tick())f.state[what]=value;}
static void STUDY_GL_CALL param(GLenum target,GLenum what,GLint value){CHECK(target==Texture2D);CHECK((what==TextureMinFilter||what==TextureMagFilter)?value==Nearest:value==ClampToEdge);tick();}
static void STUDY_GL_CALL upload(GLenum target,GLint level,GLint internal,GLsizei w,GLsizei h,GLint border,GLenum format,GLenum type,const void* ptr){
 ++f.uploads;CHECK(target==Texture2D&&level==0&&internal==RGBA8&&border==0&&format==RGBA&&type==UnsignedByte);
 if(!tick()||f.upload_error){f.error=0x0505;return;}
 CHECK(f.live.count(static_cast<GLuint>(f.state[TextureBinding2D]))==1&&f.state[PixelUnpackBufferBinding]==0&&f.state[UnpackAlignment]==1&&f.state[UnpackRowLength]==0&&f.state[UnpackSkipRows]==0&&f.state[UnpackSkipPixels]==0);
 const auto* p=static_cast<const unsigned char*>(ptr);f.copy.assign(p,p+std::size_t(w)*h*4);
}
static void STUDY_GL_CALL del(GLsizei n,const GLuint* ptr){CHECK(n==1&&f.live.erase(*ptr)==1);++f.deletes;if(f.state[TextureBinding2D]==static_cast<GLint>(*ptr))f.state[TextureBinding2D]=0;}
static GlApi texture_api(){GlApi g;g.GetError=get_error;g.GetIntegerv=get_int;g.GenTextures=gen;g.BindTexture=bind;g.BindBuffer=buffer;g.PixelStorei=store;g.TexParameteri=param;g.TexImage2D=upload;g.DeleteTextures=del;return g;}
