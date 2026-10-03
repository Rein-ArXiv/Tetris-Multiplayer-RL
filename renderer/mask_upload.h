#pragma once
#include "gl_api.h"
#include <cstdint>
#include <new>
#include <vector>
namespace text_detail {
// Single render thread/current context. Normalize tightly packed CPU R8 bytes;
// the active unit, its binding and all relevant unpack state are preserved.
class CpuUnpack {
public:
    CpuUnpack() noexcept = default;
    ~CpuUnpack() { if(saved_) restore(); }
    bool begin(GLuint texture) noexcept {
        if(saved_ || gl_GetError()) return false;
        gl_GetIntegerv(GL_TEXTURE_BINDING_2D,&bound_);
        gl_GetIntegerv(GL_PIXEL_UNPACK_BUFFER_BINDING,&pbo_);
        gl_GetIntegerv(GL_UNPACK_ALIGNMENT,&alignment_);
        gl_GetIntegerv(GL_UNPACK_ROW_LENGTH,&row_);
        gl_GetIntegerv(GL_UNPACK_SKIP_ROWS,&skip_rows_);
        gl_GetIntegerv(GL_UNPACK_SKIP_PIXELS,&skip_pixels_);
        if(gl_GetError()) return false;
        saved_=true;
        gl_BindTexture(GL_TEXTURE_2D,texture);gl_BindBuffer(GL_PIXEL_UNPACK_BUFFER,0);
        gl_PixelStorei(GL_UNPACK_ALIGNMENT,1);gl_PixelStorei(GL_UNPACK_ROW_LENGTH,0);
        gl_PixelStorei(GL_UNPACK_SKIP_ROWS,0);gl_PixelStorei(GL_UNPACK_SKIP_PIXELS,0);
        return gl_GetError()==0;
    }
    bool restore() noexcept {
        if(!saved_) return false;
        gl_BindTexture(GL_TEXTURE_2D,static_cast<GLuint>(bound_));
        gl_BindBuffer(GL_PIXEL_UNPACK_BUFFER,static_cast<GLuint>(pbo_));
        gl_PixelStorei(GL_UNPACK_ALIGNMENT,alignment_);gl_PixelStorei(GL_UNPACK_ROW_LENGTH,row_);
        gl_PixelStorei(GL_UNPACK_SKIP_ROWS,skip_rows_);gl_PixelStorei(GL_UNPACK_SKIP_PIXELS,skip_pixels_);
        saved_=false;return gl_GetError()==0;
    }
    CpuUnpack(const CpuUnpack&)=delete;
    CpuUnpack& operator=(const CpuUnpack&)=delete;
private:
    GLint bound_=0,pbo_=0,alignment_=0,row_=0,skip_rows_=0,skip_pixels_=0;
    bool saved_=false;
};


inline GLuint create_mask8(int side) {
    if(side<=0 || gl_GetError()) return 0;
    GLint limit=0;gl_GetIntegerv(GL_MAX_TEXTURE_SIZE,&limit);
    if(gl_GetError() || side>limit || side>4096) return 0;
    try {
        std::vector<std::uint8_t> zero(std::size_t(side)*side,0);
        GLuint candidate=0;gl_GenTextures(1,&candidate);
        if(gl_GetError() || !candidate){if(candidate)gl_DeleteTextures(1,&candidate);return 0;}
        CpuUnpack state;bool ok=state.begin(candidate);
        if(ok){
            gl_TexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
            gl_TexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
            gl_TexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
            gl_TexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
            ok=gl_GetError()==0;
        }
        if(ok){gl_TexImage2D(GL_TEXTURE_2D,0,GL_R8,side,side,0,GL_RED,GL_UNSIGNED_BYTE,zero.data());ok=gl_GetError()==0;}
        const bool restored=state.restore();
        if(!ok || !restored){gl_DeleteTextures(1,&candidate);return 0;}
        return candidate;
    } catch(const std::bad_alloc&){return 0;}
}
// x/y/width/height fit the allocated atlas; pixels contains width*height bytes.
inline bool update_mask8(GLuint texture,int x,int y,int width,int height,const std::uint8_t* pixels) noexcept {
    if(!texture || !pixels || x<0 || y<0 || width<=0 || height<=0)return false;
    CpuUnpack state;bool ok=state.begin(texture);
    if(ok){gl_TexSubImage2D(GL_TEXTURE_2D,0,x,y,width,height,GL_RED,GL_UNSIGNED_BYTE,pixels);ok=gl_GetError()==0;}
    const bool restored=state.restore();return ok&&restored;
}
} // namespace text_detail
