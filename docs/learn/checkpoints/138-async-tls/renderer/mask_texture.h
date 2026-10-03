#pragma once
#include "renderer/gl_api.h"
#include "text/shelf.h"
#include <cstdint>
#include <new>
#include <vector>

namespace study_atlas {
// Normalize tightly packed CPU bytes without changing the active texture unit.
class CpuUnpack {
public:
    explicit CpuUnpack(const study_gl::GlApi& gl) noexcept : gl_(gl) {}
    ~CpuUnpack() { if(saved_) restore(); }
    bool begin(study_gl::GLuint texture) noexcept {
        using namespace study_gl;
        if(saved_ || gl_.GetError()) return false;
        gl_.GetIntegerv(TextureBinding2D,&bound_);
        gl_.GetIntegerv(PixelUnpackBufferBinding,&pbo_);
        gl_.GetIntegerv(UnpackAlignment,&alignment_);
        gl_.GetIntegerv(UnpackRowLength,&row_);
        gl_.GetIntegerv(UnpackSkipRows,&skip_rows_);
        gl_.GetIntegerv(UnpackSkipPixels,&skip_pixels_);
        if(gl_.GetError()) return false;
        saved_=true;
        gl_.BindTexture(Texture2D,texture);gl_.BindBuffer(PixelUnpackBuffer,0);
        gl_.PixelStorei(UnpackAlignment,1);gl_.PixelStorei(UnpackRowLength,0);
        gl_.PixelStorei(UnpackSkipRows,0);gl_.PixelStorei(UnpackSkipPixels,0);
        return gl_.GetError()==0;
    }
    bool restore() noexcept {
        if(!saved_) return false;
        using namespace study_gl;
        gl_.BindTexture(Texture2D,static_cast<GLuint>(bound_));
        gl_.BindBuffer(PixelUnpackBuffer,static_cast<GLuint>(pbo_));
        gl_.PixelStorei(UnpackAlignment,alignment_);gl_.PixelStorei(UnpackRowLength,row_);
        gl_.PixelStorei(UnpackSkipRows,skip_rows_);gl_.PixelStorei(UnpackSkipPixels,skip_pixels_);
        saved_=false;return gl_.GetError()==0;
    }
    CpuUnpack(const CpuUnpack&)=delete;
    CpuUnpack& operator=(const CpuUnpack&)=delete;
private:
    const study_gl::GlApi& gl_;
    study_gl::GLint bound_=0,pbo_=0,alignment_=0,row_=0,skip_rows_=0,skip_pixels_=0;
    bool saved_=false;
};

// Current GL context/table outlive this owner. Only level 0, bilinear filtering.
class MaskTexture {
public:
    explicit MaskTexture(const study_gl::GlApi& gl) noexcept : gl_(gl) {}
    ~MaskTexture(){if(name_)gl_.DeleteTextures(1,&name_);}
    MaskTexture(const MaskTexture&)=delete;
    MaskTexture& operator=(const MaskTexture&)=delete;
    study_gl::GLuint name() const noexcept{return name_;}
    int width() const noexcept{return width_;}
    int height() const noexcept{return height_;}
    bool init(int width,int height) {
        using namespace study_gl;
        if(name_ || !Shelf(width,height).valid() || gl_.GetError()) return false;
        GLint limit=0;gl_.GetIntegerv(MaxTextureSize,&limit);
        if(gl_.GetError() || width>limit || height>limit) return false;
        try {
            std::vector<unsigned char> zero(std::size_t(width)*height,0);
            GLuint candidate=0;gl_.GenTextures(1,&candidate);
            if(gl_.GetError() || !candidate){if(candidate)gl_.DeleteTextures(1,&candidate);return false;}
            CpuUnpack state(gl_);bool ok=state.begin(candidate);
            if(ok){
                gl_.TexParameteri(Texture2D,TextureMinFilter,Linear);
                gl_.TexParameteri(Texture2D,TextureMagFilter,Linear);
                gl_.TexParameteri(Texture2D,TextureWrapS,ClampToEdge);
                gl_.TexParameteri(Texture2D,TextureWrapT,ClampToEdge);
                ok=gl_.GetError()==0;
            }
            if(ok){gl_.TexImage2D(Texture2D,0,R8,width,height,0,Red,UnsignedByte,zero.data());ok=gl_.GetError()==0;}
            const bool restored=state.restore();
            if(!ok || !restored){gl_.DeleteTextures(1,&candidate);return false;}
            name_=candidate;width_=width;height_=height;return true;
        } catch(const std::bad_alloc&){return false;}
    }
    bool write(Shelf::Rect r,const std::vector<unsigned char>& bytes) noexcept {
        if(!name_ || r.x<0 || r.y<0 || r.w<=0 || r.h<=0 || r.x>width_ || r.y>height_ ||
           r.w>width_-r.x || r.h>height_-r.y || bytes.size()!=std::size_t(r.w)*r.h) return false;
        using namespace study_gl;
        CpuUnpack state(gl_);bool ok=state.begin(name_);
        if(ok){gl_.TexSubImage2D(Texture2D,0,r.x,r.y,r.w,r.h,Red,UnsignedByte,bytes.data());ok=gl_.GetError()==0;}
        const bool restored=state.restore();return ok&&restored;
    }
    bool clear() {
        if(!name_) return false;
        try {return write({0,0,width_,height_},std::vector<unsigned char>(std::size_t(width_)*height_,0));}
        catch(const std::bad_alloc&){return false;}
    }
private:
    const study_gl::GlApi& gl_;
    study_gl::GLuint name_=0;
    int width_=0,height_=0;
};
} // namespace study_atlas
