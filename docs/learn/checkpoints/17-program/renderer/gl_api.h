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

struct GlApi {
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
};

// Fills `out` from `resolver`. On entry `out` is cleared, so a failed load
// never leaves a half-filled API behind.
// Returns false when `resolver` is null or any slot is missing; every missing
// name is reported on stderr. The loader makes no GL calls and invokes no
// pointer from a failed load. `out` is assigned only when all slots are valid.
bool load(GlApi& out, Resolver resolver);

} // namespace study_gl
