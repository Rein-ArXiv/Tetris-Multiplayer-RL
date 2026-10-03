#include "renderer/triangle.h"

#include <cstdio>

namespace study_gl {
namespace {

// Drains one error token and reports it. Kept local and small on purpose:
// this pass has no generic error-framework ambitions.
bool check(const GlApi& gl, const char* stage) noexcept {
    const GLenum error = gl.GetError();
    if (error == 0) {
        return true;
    }
    std::fprintf(stderr, "draw_triangle: %s failed (GL error 0x%04X)\n",
                 stage, static_cast<unsigned>(error));
    return false;
}

} // namespace

namespace {
bool valid_range(GLsizei available, GLint first, GLsizei count) noexcept {
    return available > 0 && first >= 0 && count > 0 && count % 3 == 0 &&
           first <= available && count <= available - first;
}
}

bool begin_color_frame(const GlApi& gl, int width, int height,
                       const study_blend::Rgba& background) noexcept {
    if (width <= 0 || height <= 0 || !study_blend::valid_rgba(background)) return false;
    if (!check(gl, "frame entry")) return false;
    gl.Viewport(0, 0, width, height);
    if (!check(gl, "Viewport")) return false;
    gl.ClearColor(static_cast<GLfloat>(background.r), static_cast<GLfloat>(background.g),
                  static_cast<GLfloat>(background.b), static_cast<GLfloat>(background.a));
    if (!check(gl, "ClearColor")) return false;
    gl.Clear(ColorBufferBit);
    return check(gl, "Clear");
}

bool submit_triangles(const GlApi& gl, GLuint program, GLuint vao, GLsizei available,
                      GLint first, GLsizei count) noexcept {
    if (!valid_range(available, first, count) || !program || !vao) return false;
    if (!check(gl, "submit entry")) return false;
    // --- Cleanup guard: runs on every return past entry and always unbinds.
    // Failure paths leave it armed and do not collect cleanup errors; the
    // success path performs the checked cleanup and disarms it at the end.
    struct UnbindGuard {
        const GlApi& gl;
        bool armed;
        ~UnbindGuard() noexcept {
            if (!armed) {
                return;
            }
            gl.BindVertexArray(0);
            gl.UseProgram(0);
        }
    } guard{gl, true};

    // --- Exclusive teaching pass. ---
    gl.UseProgram(program);
    if (!check(gl, "UseProgram")) {
        return false;
    }

    gl.BindVertexArray(vao);
    if (!check(gl, "BindVertexArray")) {
        return false;
    }

    gl.DrawArrays(Triangles, first, count);
    if (!check(gl, "DrawArrays")) {
        return false;
    }

    // --- Checked cleanup on success. The old program/VAO are intentionally
    // not restored (documented exclusive-pass policy). If a cleanup check
    // fails we return early with the guard still armed, so both objects are
    // unbound without collecting redundant cleanup errors. ---
    gl.BindVertexArray(0);
    if (!check(gl, "BindVertexArray(0)")) {
        return false;
    }
    gl.UseProgram(0);
    if (!check(gl, "UseProgram(0)")) {
        return false;
    }

    guard.armed = false;
    return true;
}

bool draw_triangles(const GlApi& gl, GLuint program, GLuint vao, GLsizei available,
                    GLint first, GLsizei count, int width, int height) noexcept {
    // Preserve the convenience wrapper's rejection-before-clear contract.
    if (!valid_range(available, first, count) || !program || !vao) return false;
    return begin_color_frame(gl, width, height, {0.05, 0.08, 0.12, 1.0}) &&
           submit_triangles(gl, program, vao, available, first, count);
}

bool draw_triangle(const GlApi& gl, GLuint program, GLuint vao,
                   int width, int height) noexcept {
    return draw_triangles(gl, program, vao, 3, 0, 3, width, height);
}
} // namespace study_gl
