#pragma once
#include <cstddef>

// Teaching loader for OpenGL 3.3 Core entry points.
//
// This header has no SDL dependency. The platform supplies a Resolver; the
// caller supplies a current desktop GL context and verifies 3.3 Core before
// using the buffer functions. This table includes diagnostic readback functions.
// One context per session: after the context is (re)created, reset the GlApi
// and load again: cached addresses are not assumed valid for a new context.

#if defined(_WIN32)
#define STUDY_GL_CALL __stdcall
#else
#define STUDY_GL_CALL
#endif

namespace study_gl {

using GLfloat = float;
using GLbitfield = unsigned int;
using GLchar = char;
using GLboolean = unsigned char;
using GLenum = unsigned int;
using GLint  = int;
using GLuint = unsigned int;
using GLsizei = int;
using GLsizeiptr = std::ptrdiff_t;
using GLintptr = std::ptrdiff_t;

// Constant tokens the renderer reads through the loaded pointers.
constexpr GLenum Version            = 0x1F02;
constexpr GLenum Renderer           = 0x1F01;
constexpr GLenum MaxVertexAttribs = 0x8869;
constexpr GLenum MajorVersion = 0x821B;
constexpr GLenum MinorVersion = 0x821C;
constexpr GLenum ContextProfileMask = 0x9126;
constexpr GLint CoreProfileBit = 0x00000001;

constexpr GLenum ArrayBuffer = 0x8892;
constexpr GLenum ArrayBufferBinding = 0x8894;
constexpr GLenum StaticDraw = 0x88E4;
constexpr GLenum DynamicDraw = 0x88E8;
constexpr GLenum BufferSize = 0x8764;

constexpr GLenum Float = 0x1406;
constexpr GLboolean False = 0;
constexpr GLenum VertexArrayBinding = 0x85B5;
constexpr GLenum AttribEnabled = 0x8622;
constexpr GLenum AttribSize = 0x8623;
constexpr GLenum AttribStride = 0x8624;
constexpr GLenum AttribType = 0x8625;
constexpr GLenum AttribNormalized = 0x886A;
constexpr GLenum AttribBufferBinding = 0x889F;
constexpr GLenum AttribPointer = 0x8645;

constexpr GLenum Triangles = 0x0004;
constexpr GLbitfield ColorBufferBit = 0x00004000;
constexpr GLenum RGBA = 0x1908;
constexpr GLenum Red = 0x1903;
constexpr GLenum R8 = 0x8229;
constexpr GLenum Linear = 0x2601;
constexpr GLenum UnsignedByte = 0x1401;
constexpr GLenum Front = 0x0404;
constexpr GLenum Back = 0x0405;
constexpr GLenum CullFaceCap = 0x0B44;
constexpr GLenum Blend = 0x0BE2;
constexpr GLenum FramebufferSrgb = 0x8DB9;
constexpr GLenum Dither = 0x0BD0;
constexpr GLenum FuncAdd = 0x8006;
constexpr GLenum One = 1;
constexpr GLenum SrcAlpha = 0x0302;
constexpr GLenum OneMinusSrcAlpha = 0x0303;
constexpr GLenum CounterClockwise = 0x0901;
constexpr GLenum ScissorTest = 0x0C11;
constexpr GLenum ViewportState = 0x0BA2;

constexpr GLenum VertexShader = 0x8B31;
constexpr GLenum FragmentShader = 0x8B30;
constexpr GLenum CompileStatus = 0x8B81;
constexpr GLenum LinkStatus = 0x8B82;
constexpr GLenum AttachedShaders = 0x8B85;
constexpr GLenum CurrentProgram = 0x8B8D;
constexpr GLenum InfoLogLength = 0x8B84;

// Looks up an entry point. A non-null result alone does not prove support;
// the caller also verifies the context version/features. Converting void* to a
// function pointer is the platform (SDL) contract, not a universally
// portable ISO rule.
using Resolver = void* (*)(const char*);

constexpr GLenum Texture2D = 0x0DE1;
constexpr GLenum TextureBinding2D = 0x8069;
constexpr GLenum Texture0 = 0x84C0;
constexpr GLenum ActiveTextureState = 0x84E0;
constexpr GLenum RGBA8 = 0x8058;
constexpr GLenum MaxTextureSize = 0x0D33;
constexpr GLenum TextureMinFilter = 0x2801;
constexpr GLenum TextureMagFilter = 0x2800;
constexpr GLenum TextureWrapS = 0x2802;
constexpr GLenum TextureWrapT = 0x2803;
constexpr GLenum Nearest = 0x2600;
constexpr GLenum ClampToEdge = 0x812F;
constexpr GLenum UnpackAlignment = 0x0CF5;
constexpr GLenum UnpackRowLength = 0x0CF2;
constexpr GLenum UnpackSkipRows = 0x0CF3;
constexpr GLenum UnpackSkipPixels = 0x0CF4;
constexpr GLenum PixelUnpackBuffer = 0x88EC;
constexpr GLenum PixelUnpackBufferBinding = 0x88EF;
constexpr GLenum TextureWidth = 0x1000;
constexpr GLenum TextureHeight = 0x1001;

struct GlApi {
    void (STUDY_GL_CALL* GenTextures)(GLsizei, GLuint*) = nullptr;
    void (STUDY_GL_CALL* BindTexture)(GLenum, GLuint) = nullptr;
    void (STUDY_GL_CALL* ActiveTexture)(GLenum) = nullptr;
    void (STUDY_GL_CALL* TexImage2D)(GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum, const void*) = nullptr;
    void (STUDY_GL_CALL* TexSubImage2D)(GLenum, GLint, GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, const void*) = nullptr;
    void (STUDY_GL_CALL* TexParameteri)(GLenum, GLenum, GLint) = nullptr;
    void (STUDY_GL_CALL* DeleteTextures)(GLsizei, const GLuint*) = nullptr;
    void (STUDY_GL_CALL* PixelStorei)(GLenum, GLint) = nullptr;
    void (STUDY_GL_CALL* GetTexImage)(GLenum, GLint, GLenum, GLenum, void*) = nullptr;
    void (STUDY_GL_CALL* GetTexLevelParameteriv)(GLenum, GLint, GLenum, GLint*) = nullptr;
    GLint (STUDY_GL_CALL* GetUniformLocation)(GLuint, const GLchar*) = nullptr;
    void (STUDY_GL_CALL* Uniform1i)(GLint, GLint) = nullptr;

    // Null until load() succeeds. Calling a null slot is not allowed.
    const unsigned char* (STUDY_GL_CALL* GetString)(GLenum)          = nullptr;
    void                 (STUDY_GL_CALL* GetIntegerv)(GLenum, GLint*) = nullptr;
    GLenum               (STUDY_GL_CALL* GetError)(void)             = nullptr;
    void (STUDY_GL_CALL* GenBuffers)(GLsizei, GLuint*) = nullptr;
    void (STUDY_GL_CALL* BindBuffer)(GLenum, GLuint) = nullptr;
    void (STUDY_GL_CALL* BufferData)(GLenum, GLsizeiptr, const void*, GLenum) = nullptr;
    void (STUDY_GL_CALL* DeleteBuffers)(GLsizei, const GLuint*) = nullptr;
    void (STUDY_GL_CALL* GetBufferParameteriv)(GLenum, GLenum, GLint*) = nullptr;
    void (STUDY_GL_CALL* GetBufferSubData)(GLenum, GLintptr, GLsizeiptr, void*) = nullptr;
    void (STUDY_GL_CALL* GenVertexArrays)(GLsizei, GLuint*) = nullptr;
    void (STUDY_GL_CALL* BindVertexArray)(GLuint) = nullptr;
    void (STUDY_GL_CALL* DeleteVertexArrays)(GLsizei, const GLuint*) = nullptr;
    void (STUDY_GL_CALL* VertexAttribPointer)(GLuint, GLint, GLenum, GLboolean, GLsizei, const void*) = nullptr;
    void (STUDY_GL_CALL* EnableVertexAttribArray)(GLuint) = nullptr;
    void (STUDY_GL_CALL* DisableVertexAttribArray)(GLuint) = nullptr;
    void (STUDY_GL_CALL* GetVertexAttribiv)(GLuint, GLenum, GLint*) = nullptr;
    void (STUDY_GL_CALL* GetVertexAttribPointerv)(GLuint, GLenum, void**) = nullptr;
    GLuint (STUDY_GL_CALL* CreateShader)(GLenum) = nullptr;
    void (STUDY_GL_CALL* ShaderSource)(GLuint, GLsizei, const GLchar* const*, const GLint*) = nullptr;
    void (STUDY_GL_CALL* CompileShader)(GLuint) = nullptr;
    void (STUDY_GL_CALL* GetShaderiv)(GLuint, GLenum, GLint*) = nullptr;
    void (STUDY_GL_CALL* GetShaderInfoLog)(GLuint, GLsizei, GLsizei*, GLchar*) = nullptr;
    void (STUDY_GL_CALL* DeleteShader)(GLuint) = nullptr;
    GLuint (STUDY_GL_CALL* CreateProgram)() = nullptr;
    void (STUDY_GL_CALL* AttachShader)(GLuint, GLuint) = nullptr;
    void (STUDY_GL_CALL* LinkProgram)(GLuint) = nullptr;
    void (STUDY_GL_CALL* GetProgramiv)(GLuint, GLenum, GLint*) = nullptr;
    void (STUDY_GL_CALL* GetProgramInfoLog)(GLuint, GLsizei, GLsizei*, GLchar*) = nullptr;
    void (STUDY_GL_CALL* DetachShader)(GLuint, GLuint) = nullptr;
    void (STUDY_GL_CALL* DeleteProgram)(GLuint) = nullptr;
    void (STUDY_GL_CALL* UseProgram)(GLuint) = nullptr;
    void (STUDY_GL_CALL* Scissor)(GLint, GLint, GLsizei, GLsizei) = nullptr;
    void (STUDY_GL_CALL* Viewport)(GLint, GLint, GLsizei, GLsizei) = nullptr;
    void (STUDY_GL_CALL* ClearColor)(GLfloat, GLfloat, GLfloat, GLfloat) = nullptr;
    void (STUDY_GL_CALL* Clear)(GLbitfield) = nullptr;
    void (STUDY_GL_CALL* DrawArrays)(GLenum, GLint, GLsizei) = nullptr;
    void (STUDY_GL_CALL* ReadPixels)(GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, void*) = nullptr;
    void (STUDY_GL_CALL* ReadBuffer)(GLenum) = nullptr;
    void (STUDY_GL_CALL* Enable)(GLenum) = nullptr;
    void (STUDY_GL_CALL* Disable)(GLenum) = nullptr;
    void (STUDY_GL_CALL* FrontFace)(GLenum) = nullptr;
    void (STUDY_GL_CALL* CullFace)(GLenum) = nullptr;
    void (STUDY_GL_CALL* Flush)() = nullptr;
    void (STUDY_GL_CALL* Finish)() = nullptr;
    void (STUDY_GL_CALL* BlendEquation)(GLenum) = nullptr;
    void (STUDY_GL_CALL* BlendFuncSeparate)(GLenum, GLenum, GLenum, GLenum) = nullptr;

};

// Fills `out` from `resolver`. On entry `out` is cleared, so a failed load
// never leaves a half-filled API behind.
// Returns false when `resolver` is null or any slot is missing; every missing
// name is reported on stderr. The loader makes no GL calls and invokes no
// pointer from a failed load. `out` is assigned only when all slots are valid.
bool load(GlApi& out, Resolver resolver);

} // namespace study_gl
