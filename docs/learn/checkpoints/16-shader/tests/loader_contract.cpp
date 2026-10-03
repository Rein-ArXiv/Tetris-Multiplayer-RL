#include "renderer/gl_api.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>

// These functions must never be called by the loader. The conversion below
// belongs to this platform lookup fixture, not a generic ISO pointer guarantee.
static unsigned mask, lookups, calls;
static void STUDY_GL_CALL not_called() { ++calls; }
static const char* names[] = {"glGetString", "glGetIntegerv", "glGetError", "glGenBuffers", "glBindBuffer", "glBufferData", "glDeleteBuffers", "glGetBufferParameteriv", "glGetBufferSubData", "glGenVertexArrays", "glBindVertexArray", "glDeleteVertexArrays", "glVertexAttribPointer", "glEnableVertexAttribArray", "glDisableVertexAttribArray", "glGetVertexAttribiv", "glGetVertexAttribPointerv", "glCreateShader", "glShaderSource", "glCompileShader", "glGetShaderiv", "glGetShaderInfoLog", "glDeleteShader"};
constexpr unsigned count = sizeof(names) / sizeof(names[0]);
static void require(bool ok) { if (!ok) std::exit(1); }
static void* resolve(const char* name) {
    require(lookups < count && std::strcmp(names[lookups], name) == 0);
    const unsigned bit = 1u << lookups++;
    return (mask & bit) ? nullptr : reinterpret_cast<void*>(not_called);
}
static bool empty(const study_gl::GlApi& gl) {
    return !gl.GetString && !gl.GetIntegerv && !gl.GetError && !gl.GenBuffers &&
        !gl.BindBuffer && !gl.BufferData && !gl.DeleteBuffers &&
        !gl.GetBufferParameteriv && !gl.GetBufferSubData &&
        !gl.GenVertexArrays && !gl.BindVertexArray && !gl.DeleteVertexArrays && !gl.VertexAttribPointer && !gl.EnableVertexAttribArray && !gl.DisableVertexAttribArray && !gl.GetVertexAttribiv && !gl.GetVertexAttribPointerv &&
        !gl.CreateShader && !gl.ShaderSource && !gl.CompileShader && !gl.GetShaderiv && !gl.GetShaderInfoLog && !gl.DeleteShader;
}
int main() {
    study_gl::GlApi gl;
    for (unsigned trial=0; trial<count+2; ++trial) {
        // Reload, then test no gap, each individual gap, and all gaps together.
        mask=lookups=calls=0; require(study_gl::load(gl, resolve));
        mask = trial == 0 ? 0 : trial == count+1 ? (1u<<count)-1 : 1u<<(trial-1);
        lookups=0;
        require(study_gl::load(gl, resolve) == (trial == 0));
        require(lookups == count && calls == 0);
        if (trial) require(empty(gl));
    }
    require(!study_gl::load(gl, nullptr) && empty(gl));
    std::puts("Loader: 23 individual missing slots and all-missing table, no early calls, cleared reload passed");
}
