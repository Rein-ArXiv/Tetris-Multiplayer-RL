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

bool draw_triangles(const GlApi& gl, GLuint program, GLuint vao, GLsizei available,
                    GLint first, GLsizei count, int width, int height) noexcept {
    // Subtraction after sign/order checks avoids overflowing first + count.
    // This helper chooses complete triangle groups; GL itself allows tails.
    if (available <= 0 || first < 0 || count <= 0 || count % 3 != 0 ||
        first > available || count > available - first) {
        std::fputs("draw_triangles: invalid vertex range\n", stderr);
        return false;
    }
    // --- Entry validation: no GL call has mutated anything yet. ---
    if (width <= 0 || height <= 0) {
        std::fprintf(stderr, "draw_triangle: non-positive drawable size %dx%d\n",
                     width, height);
        return false;
    }
    if (program == 0 || vao == 0) {
        std::fprintf(stderr, "draw_triangle: zero program (%u) or VAO (%u)\n",
                     static_cast<unsigned>(program), static_cast<unsigned>(vao));
        return false;
    }
    if (const GLenum stale = gl.GetError(); stale != 0) {
        std::fprintf(stderr, "draw_triangle: stale GL error 0x%04X at entry\n",
                     static_cast<unsigned>(stale));
        return false;
    }

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

    gl.Viewport(0, 0, width, height);
    if (!check(gl, "Viewport")) {
        return false;
    }

    gl.ClearColor(0.05f, 0.08f, 0.12f, 1.0f);
    if (!check(gl, "ClearColor")) {
        return false;
    }

    gl.Clear(ColorBufferBit);
    if (!check(gl, "Clear")) {
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

bool draw_triangle(const GlApi& gl, GLuint program, GLuint vao,
                   int width, int height) noexcept {
    return draw_triangles(gl, program, vao, 3, 0, 3, width, height);
}

} // namespace study_gl
