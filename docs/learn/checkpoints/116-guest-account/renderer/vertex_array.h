#pragma once
// Lesson 15: RAII ownership of one OpenGL vertex array object.
//
// VertexArray owns the GL vertex-array name only. It does not own the GL
// buffer, the CPU vertex data, the GL context, or any other GL object. The
// referenced buffer must already exist and stay alive while this object is
// used. All GlApi entry points must be loaded before construction, and both
// the GlApi table and the current GL context must outlive this object. The
// context is a single, current, desktop OpenGL 3.3 Core context.
//
// There is no bind/unbind save-restore: this object is used only at an
// exclusive initialization boundary, so ArrayBuffer and vertex-array binding
// changes are intentional.

#include <cstddef>

#include "renderer/gl_api.h"  // study_gl::GlApi, GLuint, GLenum
#include "renderer/mesh.h"    // study_mesh::Vertex2

namespace study_gl {

class VertexArray {
public:
    explicit VertexArray(const GlApi& gl) noexcept;
    ~VertexArray() noexcept;

    VertexArray(const VertexArray&) = delete;
    VertexArray& operator=(const VertexArray&) = delete;
    VertexArray(VertexArray&&) = delete;
    VertexArray& operator=(VertexArray&&) = delete;

    // First configuration only. Records `buffer`'s layout for attribute
    // location 0. Fails without touching the existing object when this VAO is
    // not empty or `buffer` is 0. On any GL failure the owned name is released
    // and the object returns to the empty state. The buffer is not owned.
    bool configure(GLuint buffer) noexcept;

    // The owned GL name, or 0 when nothing is owned.
    GLuint name() const noexcept { return name_; }

    // Deletes the owned GL name, if any, and sets it to 0. Idempotent.
    void reset() noexcept;

private:
    const GlApi& gl_;
    GLuint name_ = 0;
};

} // namespace study_gl
