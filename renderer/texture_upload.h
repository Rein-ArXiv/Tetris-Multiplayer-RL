#pragma once
#include "gl_api.h"
#include <cstdint>
#include <limits>

namespace image_detail {
// Current renderer context, readable tightly packed width*height*4 RGBA8 bytes.
// Returns an owned GL name only after upload and state restoration succeed.
inline GLuint upload_rgba8(const std::uint8_t* rgba, int width, int height) noexcept {
    if (!rgba || width<=0 || height<=0) return 0;
    const auto w=static_cast<std::size_t>(width), h=static_cast<std::size_t>(height);
    if (w>(std::numeric_limits<std::size_t>::max)()/4 ||
        h>(std::numeric_limits<std::size_t>::max)()/(w*4)) return 0;
    if (gl_GetError()) return 0;
    GLint limit = 0;
    gl_GetIntegerv(GL_MAX_TEXTURE_SIZE, &limit);
    if (gl_GetError() || limit <= 0 || width > limit || height > limit)
        return 0;
    GLint bound=0, alignment=0, row_length=0, skip_rows=0, skip_pixels=0, unpack_buffer=0;
    gl_GetIntegerv(GL_TEXTURE_BINDING_2D,&bound);
    gl_GetIntegerv(GL_UNPACK_ALIGNMENT,&alignment);
    gl_GetIntegerv(GL_UNPACK_ROW_LENGTH,&row_length);
    gl_GetIntegerv(GL_UNPACK_SKIP_ROWS,&skip_rows);
    gl_GetIntegerv(GL_UNPACK_SKIP_PIXELS,&skip_pixels);
    gl_GetIntegerv(GL_PIXEL_UNPACK_BUFFER_BINDING,&unpack_buffer);
    if (gl_GetError()) return 0;

    GLuint candidate=0;
    gl_GenTextures(1,&candidate);
    bool ok=gl_GetError()==0 && candidate!=0;
    if (ok) {
        gl_BindTexture(GL_TEXTURE_2D,candidate);
        ok=gl_GetError()==0;
    }
    if (ok) {
        // A CPU pointer must not be interpreted as an offset in a PBO.
        gl_BindBuffer(GL_PIXEL_UNPACK_BUFFER,0);
        gl_PixelStorei(GL_UNPACK_ALIGNMENT,1);
        gl_PixelStorei(GL_UNPACK_ROW_LENGTH,0);
        gl_PixelStorei(GL_UNPACK_SKIP_ROWS,0);
        gl_PixelStorei(GL_UNPACK_SKIP_PIXELS,0);
        gl_TexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
        gl_TexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
        gl_TexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
        gl_TexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
        ok=gl_GetError()==0;
    }
    if (ok) {
        gl_TexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,width,height,0,
                       GL_RGBA,GL_UNSIGNED_BYTE,rgba);
        ok=gl_GetError()==0;
    }
    gl_BindTexture(GL_TEXTURE_2D,static_cast<GLuint>(bound));
    gl_BindBuffer(GL_PIXEL_UNPACK_BUFFER,static_cast<GLuint>(unpack_buffer));
    gl_PixelStorei(GL_UNPACK_ALIGNMENT,alignment);
    gl_PixelStorei(GL_UNPACK_ROW_LENGTH,row_length);
    gl_PixelStorei(GL_UNPACK_SKIP_ROWS,skip_rows);
    gl_PixelStorei(GL_UNPACK_SKIP_PIXELS,skip_pixels);
    const bool restored=gl_GetError()==0;
    if (!ok || !restored) {
        if (candidate) gl_DeleteTextures(1,&candidate);
        return 0;
    }
    return candidate;
}
} // namespace image_detail
