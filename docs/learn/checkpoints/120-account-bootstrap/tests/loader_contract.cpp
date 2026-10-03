#include "renderer/gl_api.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>

// These functions must never be called by the loader. The conversion below
// belongs to this platform lookup fixture, not a generic ISO pointer guarantee.
static int missing = -1; // -1: none, -2: all, otherwise one slot index.
static unsigned lookups, calls;
static void STUDY_GL_CALL not_called() { ++calls; }
static const char* names[] = {"glGetString", "glGetIntegerv", "glGetError", "glGenBuffers", "glBindBuffer", "glBufferData", "glDeleteBuffers", "glGetBufferParameteriv", "glGetBufferSubData", "glGenVertexArrays", "glBindVertexArray", "glDeleteVertexArrays", "glVertexAttribPointer", "glEnableVertexAttribArray", "glDisableVertexAttribArray", "glGetVertexAttribiv", "glGetVertexAttribPointerv", "glCreateShader", "glShaderSource", "glCompileShader", "glGetShaderiv", "glGetShaderInfoLog", "glDeleteShader", "glCreateProgram", "glAttachShader", "glLinkProgram", "glGetProgramiv", "glGetProgramInfoLog", "glDetachShader", "glDeleteProgram", "glUseProgram", "glViewport", "glClearColor", "glClear", "glDrawArrays", "glReadPixels", "glReadBuffer", "glEnable", "glDisable", "glFrontFace", "glCullFace", "glBlendEquation", "glBlendFuncSeparate", "glFlush", "glFinish", "glScissor", "glGenTextures", "glBindTexture", "glActiveTexture", "glTexImage2D", "glTexSubImage2D", "glTexParameteri", "glDeleteTextures", "glPixelStorei", "glGetTexImage", "glGetTexLevelParameteriv", "glGetUniformLocation", "glUniform1i"};
constexpr unsigned count = sizeof(names) / sizeof(names[0]);
static void require(bool ok) { if (!ok) std::exit(1); }
static void* resolve(const char* name) {
    require(lookups < count && std::strcmp(names[lookups], name) == 0);
    const unsigned index = lookups++;
    return (missing == -2 || missing == static_cast<int>(index)) ? nullptr : reinterpret_cast<void*>(not_called);
}
static bool empty(const study_gl::GlApi& gl) {
    return !gl.GetString && !gl.GetIntegerv && !gl.GetError && !gl.GenBuffers &&
        !gl.BindBuffer && !gl.BufferData && !gl.DeleteBuffers &&
        !gl.GetBufferParameteriv && !gl.GetBufferSubData &&
        !gl.GenVertexArrays && !gl.BindVertexArray && !gl.DeleteVertexArrays && !gl.VertexAttribPointer && !gl.EnableVertexAttribArray && !gl.DisableVertexAttribArray && !gl.GetVertexAttribiv && !gl.GetVertexAttribPointerv &&
        !gl.CreateShader && !gl.ShaderSource && !gl.CompileShader && !gl.GetShaderiv && !gl.GetShaderInfoLog && !gl.DeleteShader && !gl.CreateProgram && !gl.AttachShader && !gl.LinkProgram && !gl.GetProgramiv && !gl.GetProgramInfoLog && !gl.DetachShader && !gl.DeleteProgram && !gl.UseProgram && !gl.Viewport && !gl.ClearColor && !gl.Clear && !gl.DrawArrays && !gl.ReadPixels && !gl.ReadBuffer && !gl.Enable && !gl.Disable && !gl.FrontFace && !gl.CullFace && !gl.BlendEquation && !gl.BlendFuncSeparate && !gl.Flush && !gl.Finish && !gl.Scissor && !gl.GenTextures && !gl.BindTexture && !gl.ActiveTexture && !gl.TexImage2D && !gl.TexSubImage2D && !gl.TexParameteri && !gl.DeleteTextures && !gl.PixelStorei && !gl.GetTexImage && !gl.GetTexLevelParameteriv && !gl.GetUniformLocation && !gl.Uniform1i ;
}
int main() {
    study_gl::GlApi gl;
    for (unsigned trial=0; trial<count+2; ++trial) {
        // Reload, then test no gap, each individual gap, and all gaps together.
        missing=-1; lookups=calls=0; require(study_gl::load(gl, resolve));
        missing = trial == 0 ? -1 : trial == count+1 ? -2 : static_cast<int>(trial-1);
        lookups=0;
        require(study_gl::load(gl, resolve) == (trial == 0));
        require(lookups == count && calls == 0);
        if (trial) require(empty(gl));
    }
    require(!study_gl::load(gl, nullptr) && empty(gl));
    std::puts("Loader: 58 individual missing slots and all-missing table, no early calls, cleared reload passed");
}
