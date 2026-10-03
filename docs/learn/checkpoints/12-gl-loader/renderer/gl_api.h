#pragma once

// Teaching loader for OpenGL 3.3 Core entry points.
//
// This header has no SDL dependency. The platform supplies a Resolver; the
// caller supplies a current desktop GL context. These three query functions
// are core since GL 1.x; the caller checks 3.3 Core before later rendering.
// One context per session: after the context is (re)created, reset the GlApi
// and load again: cached addresses are not assumed valid for a new context.

#if defined(_WIN32)
#define STUDY_GL_CALL __stdcall
#else
#define STUDY_GL_CALL
#endif

namespace study_gl {

using GLenum = unsigned int;
using GLint  = int;

// Constant tokens the renderer reads through the loaded pointers.
constexpr GLenum Version            = 0x1F02;
constexpr GLenum Renderer           = 0x1F01;
constexpr GLenum MaxVertexAttribs = 0x8869;
constexpr GLenum MajorVersion = 0x821B;
constexpr GLenum MinorVersion = 0x821C;
constexpr GLenum ContextProfileMask = 0x9126;
constexpr GLint CoreProfileBit = 0x00000001;

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
};

// Fills `out` from `resolver`. On entry `out` is cleared, so a failed load
// never leaves a half-filled API behind.
// Returns false when `resolver` is null or any slot is missing; every missing
// name is reported on stderr. The loader makes no GL calls and invokes no
// pointer from a failed load. `out` is assigned only when all slots are valid.
bool load(GlApi& out, Resolver resolver);

} // namespace study_gl
