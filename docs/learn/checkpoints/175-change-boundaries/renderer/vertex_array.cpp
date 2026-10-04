#include "renderer/vertex_array.h"

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
    std::fprintf(stderr, "VertexArray: GL error 0x%04X after %s\n",
                 static_cast<unsigned>(err), stage);
    return false;
}

} // namespace

VertexArray::VertexArray(const GlApi& gl) noexcept : gl_(gl) {}

VertexArray::~VertexArray() noexcept { reset(); }

bool VertexArray::configure(GLuint buffer) noexcept {
    // First configuration only: never re-record an existing VAO.
    if (name_ != 0) {
        std::fprintf(stderr, "VertexArray::configure: vertex array is not empty\n");
        return false;
    }

    // A VAO records buffer bindings, so a missing buffer is rejected before
    // any GL call. Core-profile vertex arrays read from buffer objects.
    if (buffer == 0) {
        std::fprintf(stderr, "VertexArray::configure: buffer is 0\n");
        return false;
    }

    // Require a clean entry boundary. Report a pending error before we
    // allocate. This consumes one error flag; it is not an exhaustive log.
    if (!stage_ok(gl_, "entry (pre-existing error)")) return false;

    // The stride is the size of one interleaved vertex. sizeof yields size_t,
    // so guard the conversion to the signed GLsizei the API takes.
    const std::size_t stride = sizeof(study_mesh::Vertex2);
    if (stride > static_cast<std::size_t>(std::numeric_limits<GLsizei>::max())) {
        std::fprintf(stderr, "VertexArray::configure: stride too large for GL\n");
        return false;
    }

    gl_.GenVertexArrays(1, &name_);
    if (!stage_ok(gl_, "glGenVertexArrays") || name_ == 0) {
        if (name_ == 0) {
            std::fprintf(stderr, "VertexArray::configure: glGenVertexArrays returned 0\n");
        }
        reset();
        return false;
    }

    // Binding the vertex array and the ArrayBuffer is intentionally left in
    // place; there is no binding restoration promised at this initialization
    // boundary. Binding 0 would not delete the objects anyway.
    gl_.BindVertexArray(name_);
    if (!stage_ok(gl_, "glBindVertexArray")) {
        reset();
        return false;
    }

    gl_.BindBuffer(ArrayBuffer, buffer);
    if (!stage_ok(gl_, "glBindBuffer")) {
        reset();
        return false;
    }

    // Attribute location 0 is two floats, tightly packed (stride =
    // sizeof(Vertex2), offset 0). The recorded binding belongs to the
    // buffer selected when VertexAttribPointer runs; the VAO does not own it.
    gl_.VertexAttribPointer(0, 2, Float, False,
                            static_cast<GLsizei>(stride), nullptr);
    if (!stage_ok(gl_, "glVertexAttribPointer")) {
        reset();
        return false;
    }

    gl_.EnableVertexAttribArray(0);
    if (!stage_ok(gl_, "glEnableVertexAttribArray")) {
        reset();
        return false;
    }

    return true;
}

void VertexArray::reset() noexcept {
    if (name_ != 0) {
        gl_.DeleteVertexArrays(1, &name_);
        name_ = 0;
    }
}

} // namespace study_gl
