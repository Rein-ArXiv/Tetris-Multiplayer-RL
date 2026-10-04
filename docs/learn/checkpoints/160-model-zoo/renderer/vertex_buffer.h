#pragma once
// Lesson 14: RAII ownership of one OpenGL buffer object.
//
// VertexBuffer owns the GL buffer name only. It does not own the CPU vertex
// data, the GL context, or any other GL object. All GlApi entry points must be
// loaded before this object is constructed, and both the GlApi table and the
// current GL context must outlive it. The context is a single, current,
// desktop OpenGL 3.3 Core context.
//
// There is no bind/unbind save-restore: this object is used only at an
// exclusive initialization/update boundary, so ArrayBuffer binding changes are
// intentional.

#include <cstddef>

#include "renderer/gl_api.h"         // study_gl::GlApi, GLuint, GLenum, GLsizeiptr
#include "renderer/mesh.h"  // study_mesh::Triangle, study_mesh::byte_count

namespace study_gl {

class VertexBuffer {
public:
    explicit VertexBuffer(const GlApi& gl) noexcept;
    ~VertexBuffer() noexcept;

    VertexBuffer(const VertexBuffer&) = delete;
    VertexBuffer& operator=(const VertexBuffer&) = delete;
    VertexBuffer(VertexBuffer&&) = delete;
    VertexBuffer& operator=(VertexBuffer&&) = delete;

    // First upload only. Fails without touching the existing object when
    // name() != 0. On any GL failure the name is released and the object
    // returns to the empty state.
    // Caller supplies a readable contiguous array of count Vertex2 values.
    // The pointer is borrowed only until upload returns. Empty input is rejected.
    bool upload(const study_mesh::Vertex2* vertices, std::size_t count) noexcept;
    template<std::size_t N>
    bool upload(const std::array<study_mesh::Vertex2, N>& vertices) noexcept {
        return upload(vertices.data(), vertices.size());
    }
    // Replace an existing store with exactly the current vertex count.
    // Keeps the GL object name, so its VAO association stays valid.
    // Invalid arguments perform no GL work. On a GL-stage failure, the caller
    // must stop rendering; this API does not promise rollback of GPU storage.
    // Same context, exclusive binding boundary, readable extent as for upload.
    bool replace_same_size(const study_mesh::Vertex2* vertices,
                           std::size_t count) noexcept;

    GLsizei vertex_count() const noexcept { return count_; }

    // The owned GL name, or 0 when nothing is owned.
    GLuint name() const noexcept { return name_; }

    // Deletes the owned GL name, if any, and sets it to 0. Idempotent.
    void reset() noexcept;

private:
    const GlApi& gl_;
    GLuint name_ = 0;
    GLsizei count_ = 0;
};

} // namespace study_gl
