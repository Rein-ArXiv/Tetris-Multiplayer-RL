#pragma once
// Lesson 17: RAII ownership of one linked OpenGL program object.
//
// Program owns the GL program name only. It does not own the GL context, the
// GlApi table, or the shader objects it links. The table and current context
// must outlive it; a desktop OpenGL 3.3 Core context must be current and the
// GlApi entry points loaded before link() is called. The two shader handles are
// borrowed for the duration of link() only and are never deleted here: the
// caller keeps ownership of its Shader objects.
//
// Program does not bind itself. Linking is independent of whatever program is
// current, and a successful link leaves the current program untouched. The
// owned program must NOT be current when reset() or the destructor runs:
// this teaching policy permits immediate release. GL itself permits deletion
// of a current program, but defers it until it is no longer in use. Select
// another program or call UseProgram(0) first.
//
// A Program holds at most one linked program. Reuse requires reset() (or
// destruction) before linking again.

#include <string>

#include "renderer/gl_api.h"  // study_gl::GlApi, GLenum, GLuint

namespace study_gl {

class Program {
public:
    explicit Program(const GlApi& gl) noexcept;
    ~Program() noexcept;

    Program(const Program&) = delete;
    Program& operator=(const Program&) = delete;
    Program(Program&&) = delete;
    Program& operator=(Program&&) = delete;

    // Links `vertex` and `fragment` into one program. This is not noexcept:
    // validation and diagnostics may allocate.
    //
    // Fails without touching the existing object when a program is already
    // owned. Input errors (zero or equal shader names) are rejected before any
    // GL call. On any GL or link failure the owned program is released and the
    // object returns to the empty state; the borrowed shaders are never
    // deleted. A successful link that emits log text returns true and leaves
    // that text in diagnostic().
    // Precondition: one compiled vertex and one compiled fragment shader from
    // the same context/share group; nonzero names alone do not prove their type.
    bool link(GLuint vertex, GLuint fragment);

    // The owned GL name, or 0 when nothing is owned.
    GLuint name() const noexcept { return name_; }

    // Text from the last link attempt: an API failure, a link failure, or
    // notes from a successful link; empty when there is nothing to say.
    const std::string& diagnostic() const noexcept { return diagnostic_; }

    // Deletes the owned GL name, if any, and sets it to 0. Idempotent. Does not
    // call UseProgram. The diagnostic text is left in place for inspection.
    void reset() noexcept;

private:
    const GlApi& gl_;
    GLuint name_ = 0;
    std::string diagnostic_;
};

} // namespace study_gl
