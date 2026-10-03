#include "renderer/vertex_buffer.h"

#include <cstddef>
#include <cstdio>
#include <limits>

namespace study_gl {
namespace {

// Coarse, single-threaded diagnostic: read one error flag, report it with the
// stage name, and say whether the stage was clean. glGetError is a
// context-wide diagnostic, not a thread-local exception mechanism, so it is
// never driven in an unbounded loop here.
bool stage_ok(const GlApi& gl, const char* stage) noexcept {
    const GLenum err = gl.GetError();
    if (err == 0) {
        return true;
    }
    std::fprintf(stderr, "VertexBuffer: GL error 0x%04X after %s\n",
                 static_cast<unsigned>(err), stage);
    return false;
}

} // namespace

VertexBuffer::VertexBuffer(const GlApi& gl) noexcept : gl_(gl) {}

VertexBuffer::~VertexBuffer() noexcept { reset(); }

bool VertexBuffer::upload(const study_mesh::Vertex2* vertices, std::size_t count) noexcept {
    // First upload only: never reallocate or replace an existing data store.
    if (name_ != 0) {
        std::fprintf(stderr, "VertexBuffer::upload: buffer is not empty\n");
        return false;
    }

    // Validate counts BEFORE multiplication or narrowing. A pointer alone
    // cannot prove the actual readable extent; the array overload carries it.
    if (!vertices || count == 0 ||
        count > static_cast<std::size_t>(std::numeric_limits<GLsizei>::max()) ||
        count > static_cast<std::size_t>(std::numeric_limits<GLsizeiptr>::max()) /
                    sizeof(study_mesh::Vertex2)) {
        std::fputs("VertexBuffer::upload: invalid vertex extent\n", stderr);
        return false;
    }
    const std::size_t bytes = count * sizeof(study_mesh::Vertex2);
    if (!stage_ok(gl_, "entry (pre-existing error)")) return false;

    gl_.GenBuffers(1, &name_);
    if (!stage_ok(gl_, "glGenBuffers") || name_ == 0) {
        if (name_ == 0) {
            std::fprintf(stderr, "VertexBuffer::upload: glGenBuffers returned 0\n");
        }
        reset();
        return false;
    }

    // Binding ArrayBuffer is intentionally left in place; there is no
    // binding restoration promised at this initialization boundary. Binding 0
    // would not delete the object anyway.
    gl_.BindBuffer(ArrayBuffer, name_);
    if (!stage_ok(gl_, "glBindBuffer")) {
        reset();
        return false;
    }

    // Creates (or replaces) this buffer's data store and copies the initial
    // data. The copy is finished when this returns, so vertices may be
    // released immediately; the GPU may still read the store internally.
    gl_.BufferData(ArrayBuffer, static_cast<GLsizeiptr>(bytes),
                  vertices, StaticDraw);
    if (!stage_ok(gl_, "glBufferData")) {
        reset();
        return false;
    }

    count_ = static_cast<GLsizei>(count);
    return true;
}

bool VertexBuffer::replace_same_size(const study_mesh::Vertex2* vertices,
                                     std::size_t count) noexcept {
    if (name_ == 0 || !vertices || count_ <= 0 ||
        count != static_cast<std::size_t>(count_)) {
        return false;
    }
    // count_ was range-checked by upload; byte multiplication remains safe.
    const auto bytes = static_cast<GLsizeiptr>(count * sizeof(study_mesh::Vertex2));
    if (!stage_ok(gl_, "replace entry (pre-existing error)")) return false;
    gl_.BindBuffer(ArrayBuffer, name_);
    if (!stage_ok(gl_, "replace glBindBuffer")) return false;
    gl_.BufferData(ArrayBuffer, bytes, vertices, DynamicDraw);
    return stage_ok(gl_, "replace glBufferData");
}

void VertexBuffer::reset() noexcept {
    if (name_ != 0) {
        gl_.DeleteBuffers(1, &name_);
        name_ = 0;
    }
    count_ = 0;
}

} // namespace study_gl
