#include "renderer/gl_api.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>

// These functions must never be called by the loader. The conversion below
// belongs to this platform lookup fixture, not a generic ISO pointer guarantee.
static unsigned mask, lookups, calls;
static void STUDY_GL_CALL not_called() { ++calls; }
static const char* names[] = {"glGetString", "glGetIntegerv", "glGetError",
    "glGenBuffers", "glBindBuffer", "glBufferData", "glDeleteBuffers",
    "glGetBufferParameteriv", "glGetBufferSubData"};
static void require(bool ok) { if (!ok) std::exit(1); }
static void* resolve(const char* name) {
    require(lookups < 9 && std::strcmp(names[lookups], name) == 0);
    const unsigned bit = 1u << lookups++;
    return (mask & bit) ? nullptr : reinterpret_cast<void*>(not_called);
}
static bool empty(const study_gl::GlApi& gl) {
    return !gl.GetString && !gl.GetIntegerv && !gl.GetError && !gl.GenBuffers &&
        !gl.BindBuffer && !gl.BufferData && !gl.DeleteBuffers &&
        !gl.GetBufferParameteriv && !gl.GetBufferSubData;
}
int main() {
    study_gl::GlApi gl;
    for (unsigned trial=0; trial<512; ++trial) {
        // Successful reload first; then every missing combination must clear it.
        mask=lookups=calls=0; require(study_gl::load(gl, resolve));
        mask=trial; lookups=0;
        require(study_gl::load(gl, resolve) == (trial == 0));
        require(lookups == 9 && calls == 0);
        if (trial) require(empty(gl));
    }
    require(!study_gl::load(gl, nullptr) && empty(gl));
    std::puts("Loader: 512 missing-slot combinations, no early calls, cleared reload passed");
}
