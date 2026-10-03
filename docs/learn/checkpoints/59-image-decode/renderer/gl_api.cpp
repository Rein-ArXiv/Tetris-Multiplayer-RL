#include "gl_api.h"

#include <cstdio>

namespace study_gl {
namespace {

// Resolves one slot. `Fn` is deduced as the concrete function-pointer type
// (never a reference), so the cast target is exact and keeps its convention.
template <typename Fn>
bool resolve_slot(Fn& slot, Resolver resolver, const char* name)
{
    void* raw = resolver(name);
    if (raw == nullptr) {
        // Report this name and keep going, so the caller sees every gap.
        std::fprintf(stderr, "[GL] missing entry point: %s\n", name);
        return false;
    }
    // `Fn` is the value type of the slot. The void* -> function-pointer conversion is the
    // platform (SDL) contract, not a universally portable ISO rule.
    slot = reinterpret_cast<Fn>(raw);
    return true;
}

} // namespace

bool load(GlApi& out, Resolver resolver)
{
    // Clear first: a previous successful load must not survive a new attempt.
    out = GlApi{};

    if (resolver == nullptr) {
        std::fprintf(stderr, "[GL] load called with a null resolver.\n");
        return false;
    }

    // Fill a local candidate. `out` stays cleared unless every slot resolves.
    GlApi candidate;
    bool ok = true;
    ok = resolve_slot(candidate.GetString,   resolver, "glGetString") && ok;
    ok = resolve_slot(candidate.GetIntegerv, resolver, "glGetIntegerv") && ok;
    ok = resolve_slot(candidate.GetError,    resolver, "glGetError") && ok;

    ok = resolve_slot(candidate.GenBuffers, resolver, "glGenBuffers") && ok;
    ok = resolve_slot(candidate.BindBuffer, resolver, "glBindBuffer") && ok;
    ok = resolve_slot(candidate.BufferData, resolver, "glBufferData") && ok;
    ok = resolve_slot(candidate.DeleteBuffers, resolver, "glDeleteBuffers") && ok;
    ok = resolve_slot(candidate.GetBufferParameteriv, resolver, "glGetBufferParameteriv") && ok;
    ok = resolve_slot(candidate.GetBufferSubData, resolver, "glGetBufferSubData") && ok;

    ok = resolve_slot(candidate.GenVertexArrays, resolver, "glGenVertexArrays") && ok;
    ok = resolve_slot(candidate.BindVertexArray, resolver, "glBindVertexArray") && ok;
    ok = resolve_slot(candidate.DeleteVertexArrays, resolver, "glDeleteVertexArrays") && ok;
    ok = resolve_slot(candidate.VertexAttribPointer, resolver, "glVertexAttribPointer") && ok;
    ok = resolve_slot(candidate.EnableVertexAttribArray, resolver, "glEnableVertexAttribArray") && ok;
    ok = resolve_slot(candidate.DisableVertexAttribArray, resolver, "glDisableVertexAttribArray") && ok;
    ok = resolve_slot(candidate.GetVertexAttribiv, resolver, "glGetVertexAttribiv") && ok;
    ok = resolve_slot(candidate.GetVertexAttribPointerv, resolver, "glGetVertexAttribPointerv") && ok;

    ok = resolve_slot(candidate.CreateShader, resolver, "glCreateShader") && ok;
    ok = resolve_slot(candidate.ShaderSource, resolver, "glShaderSource") && ok;
    ok = resolve_slot(candidate.CompileShader, resolver, "glCompileShader") && ok;
    ok = resolve_slot(candidate.GetShaderiv, resolver, "glGetShaderiv") && ok;
    ok = resolve_slot(candidate.GetShaderInfoLog, resolver, "glGetShaderInfoLog") && ok;
    ok = resolve_slot(candidate.DeleteShader, resolver, "glDeleteShader") && ok;

    ok = resolve_slot(candidate.CreateProgram, resolver, "glCreateProgram") && ok;
    ok = resolve_slot(candidate.AttachShader, resolver, "glAttachShader") && ok;
    ok = resolve_slot(candidate.LinkProgram, resolver, "glLinkProgram") && ok;
    ok = resolve_slot(candidate.GetProgramiv, resolver, "glGetProgramiv") && ok;
    ok = resolve_slot(candidate.GetProgramInfoLog, resolver, "glGetProgramInfoLog") && ok;
    ok = resolve_slot(candidate.DetachShader, resolver, "glDetachShader") && ok;
    ok = resolve_slot(candidate.DeleteProgram, resolver, "glDeleteProgram") && ok;
    ok = resolve_slot(candidate.UseProgram, resolver, "glUseProgram") && ok;

    ok = resolve_slot(candidate.Viewport, resolver, "glViewport") && ok;
    ok = resolve_slot(candidate.ClearColor, resolver, "glClearColor") && ok;
    ok = resolve_slot(candidate.Clear, resolver, "glClear") && ok;
    ok = resolve_slot(candidate.DrawArrays, resolver, "glDrawArrays") && ok;
    ok = resolve_slot(candidate.ReadPixels, resolver, "glReadPixels") && ok;
    ok = resolve_slot(candidate.ReadBuffer, resolver, "glReadBuffer") && ok;

    ok = resolve_slot(candidate.Enable, resolver, "glEnable") && ok;
    ok = resolve_slot(candidate.Disable, resolver, "glDisable") && ok;
    ok = resolve_slot(candidate.FrontFace, resolver, "glFrontFace") && ok;
    ok = resolve_slot(candidate.CullFace, resolver, "glCullFace") && ok;

    ok = resolve_slot(candidate.BlendEquation, resolver, "glBlendEquation") && ok;
    ok = resolve_slot(candidate.BlendFuncSeparate, resolver, "glBlendFuncSeparate") && ok;

    ok = resolve_slot(candidate.Flush, resolver, "glFlush") && ok;
    ok = resolve_slot(candidate.Finish, resolver, "glFinish") && ok;

    ok = resolve_slot(candidate.Scissor, resolver, "glScissor") && ok;

    ok = resolve_slot(candidate.GenTextures, resolver, "glGenTextures") && ok;
    ok = resolve_slot(candidate.BindTexture, resolver, "glBindTexture") && ok;
    ok = resolve_slot(candidate.ActiveTexture, resolver, "glActiveTexture") && ok;
    ok = resolve_slot(candidate.TexImage2D, resolver, "glTexImage2D") && ok;
    ok = resolve_slot(candidate.TexParameteri, resolver, "glTexParameteri") && ok;
    ok = resolve_slot(candidate.DeleteTextures, resolver, "glDeleteTextures") && ok;
    ok = resolve_slot(candidate.PixelStorei, resolver, "glPixelStorei") && ok;
    ok = resolve_slot(candidate.GetTexImage, resolver, "glGetTexImage") && ok;
    ok = resolve_slot(candidate.GetTexLevelParameteriv, resolver, "glGetTexLevelParameteriv") && ok;
    ok = resolve_slot(candidate.GetUniformLocation, resolver, "glGetUniformLocation") && ok;
    ok = resolve_slot(candidate.Uniform1i, resolver, "glUniform1i") && ok;

    if (!ok) {
        return false; // candidate is discarded; `out` remains cleared.
    }

    out = candidate; // all slots valid: publish at once.
    return true;
}

} // namespace study_gl
