#pragma once
// Lesson 16: RAII ownership of one compiled OpenGL shader object.
//
// Shader owns the GL shader name only. It does not own the GL context, the
// GlApi table, or the GLSL source text. The table and current context must
// outlive it; source bytes are copied by ShaderSource and may then expire. The
// GlApi entry points must be loaded and a desktop OpenGL 3.3 Core context must
// be current before compile() is called. There is no linking, program
// creation, or drawing here; that belongs to the caller.
//
// A Shader holds at most one compiled stage. Reuse requires reset() (or
// destruction) before compiling again.

#include <string>
#include <string_view>

#include "renderer/gl_api.h"  // study_gl::GlApi, GLenum, GLuint

namespace study_gl {

class Shader {
public:
    explicit Shader(const GlApi& gl) noexcept;
    ~Shader() noexcept;

    Shader(const Shader&) = delete;
    Shader& operator=(const Shader&) = delete;
    Shader(Shader&&) = delete;
    Shader& operator=(Shader&&) = delete;

    // Compiles one VertexShader or FragmentShader from `source`. This is not
    // noexcept: validation and diagnostics may allocate.
    //
    // Fails without touching the existing object when a shader is already
    // owned. Input errors (unsupported type, empty source, source larger than
    // the GLint length limit) are rejected before any GL call. On any GL or
    // GLSL failure the owned name is released and the object returns to the
    // empty state. A successful compile that emits warnings returns true and
    // leaves those warnings in diagnostic().
    bool compile(GLenum type, std::string_view source);

    // The owned GL name, or 0 when nothing is owned.
    GLuint name() const noexcept { return name_; }

    // Text from the last compile attempt: an API failure, a GLSL failure, or
    // notes from a successful compile; empty when there is nothing to say.
    const std::string& diagnostic() const noexcept { return diagnostic_; }

    // Deletes the owned GL name, if any, and sets it to 0. Idempotent. The
    // diagnostic text is left in place for inspection.
    void reset() noexcept;

private:
    const GlApi& gl_;
    GLuint name_ = 0;
    std::string diagnostic_;
};

} // namespace study_gl
