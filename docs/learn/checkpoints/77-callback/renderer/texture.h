#pragma once
#include "renderer/gl_api.h"
#include "renderer/rgba_pixels.h"
namespace study_texture {
// GL table/current context outlive this single owner. One immutable upload.
// The caller ends all CPU queues that reference this name before destruction.
class Texture {
public:
    explicit Texture(const study_gl::GlApi& gl) noexcept : gl_(gl) {}
    ~Texture() noexcept { if (name_) gl_.DeleteTextures(1, &name_); }
    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;
    study_gl::GLuint name() const noexcept { return name_; }
    int width() const noexcept { return width_; }
    int height() const noexcept { return height_; }
    bool upload(RgbaView pixels) noexcept {
        using namespace study_gl;
        if (name_ || !valid(pixels)) return false;
        if (gl_.GetError()) return false;
        GLint limit = 0;
        gl_.GetIntegerv(MaxTextureSize, &limit);
        if (gl_.GetError() || limit <= 0 || pixels.width > limit || pixels.height > limit)
            return false;
        GLint bound=0, alignment=0, row_length=0, skip_rows=0, skip_pixels=0, unpack_buffer=0;
        gl_.GetIntegerv(TextureBinding2D,&bound);
        gl_.GetIntegerv(UnpackAlignment,&alignment);
        gl_.GetIntegerv(UnpackRowLength,&row_length);
        gl_.GetIntegerv(UnpackSkipRows,&skip_rows);
        gl_.GetIntegerv(UnpackSkipPixels,&skip_pixels);
        gl_.GetIntegerv(PixelUnpackBufferBinding,&unpack_buffer);
        if (gl_.GetError()) return false;

        GLuint candidate=0;
        gl_.GenTextures(1,&candidate);
        bool ok=gl_.GetError()==0 && candidate!=0;
        if (ok) {
            gl_.BindTexture(Texture2D,candidate);
            ok=gl_.GetError()==0;
        }
        if (ok) {
            // A CPU pointer must not be interpreted as an offset in a PBO.
            gl_.BindBuffer(PixelUnpackBuffer,0);
            gl_.PixelStorei(UnpackAlignment,1);
            gl_.PixelStorei(UnpackRowLength,0);
            gl_.PixelStorei(UnpackSkipRows,0);
            gl_.PixelStorei(UnpackSkipPixels,0);
            gl_.TexParameteri(Texture2D,TextureMinFilter,Nearest);
            gl_.TexParameteri(Texture2D,TextureMagFilter,Nearest);
            gl_.TexParameteri(Texture2D,TextureWrapS,ClampToEdge);
            gl_.TexParameteri(Texture2D,TextureWrapT,ClampToEdge);
            ok=gl_.GetError()==0;
        }
        if (ok) {
            gl_.TexImage2D(Texture2D,0,RGBA8,pixels.width,pixels.height,0,
                           RGBA,UnsignedByte,pixels.data);
            ok=gl_.GetError()==0;
        }
        gl_.BindTexture(Texture2D,static_cast<GLuint>(bound));
        gl_.BindBuffer(PixelUnpackBuffer,static_cast<GLuint>(unpack_buffer));
        gl_.PixelStorei(UnpackAlignment,alignment);
        gl_.PixelStorei(UnpackRowLength,row_length);
        gl_.PixelStorei(UnpackSkipRows,skip_rows);
        gl_.PixelStorei(UnpackSkipPixels,skip_pixels);
        const bool restored=gl_.GetError()==0;
        if (!ok || !restored) {
            if (candidate) gl_.DeleteTextures(1,&candidate);
            return false;
        }
        name_=candidate; width_=pixels.width; height_=pixels.height;
        return true;
    }
private:
    const study_gl::GlApi& gl_;
    study_gl::GLuint name_=0;
    int width_=0, height_=0;
};
} // namespace study_texture
