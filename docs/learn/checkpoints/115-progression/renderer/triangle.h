#pragma once
#include "renderer/gl_api.h"
#include "renderer/blend.h"
namespace study_gl {
// Exclusive teaching pass: successfully loaded GL3.3 Core table, current context,
// default framebuffer, no scissor/depth/stencil/rasterizer discard; FILL polygon
// mode, all color channels writable. Caller selects blending/culling and sRGB.
// No resource creation/destruction or buffer presentation in these helpers.
// begin_color_frame sets viewport and clears the color buffer ONCE per frame.
// Clear is not restricted by viewport. No prior state is saved or restored.
// The raw stored background is normalized RGBA; callers that accumulate
// source-over choose a premultiplied destination (black alpha0 or alpha1 here).
bool begin_color_frame(const GlApi& gl, int width, int height,
                       const study_blend::Rgba& background) noexcept;
// available is caller metadata matching the buffer captured by vao.
// Reads whole triangle groups from first/count in vertex units; rejects bad
// ranges/names before GL calls. Does NOT clear, set viewport, or change blending.
// Binds program/VAO then unbinds both (0); previous bindings are not restored.
// Failure attempts unbinding but false does not certify restoration.
bool submit_triangles(const GlApi& gl, GLuint program, GLuint vao, GLsizei available,
                      GLint first, GLsizei count) noexcept;
// Compatibility helpers for earlier one-shot demos. Each call clears, then
// submits. New multi-draw callers use begin_color_frame once and submit twice.
bool draw_triangles(const GlApi& gl, GLuint program, GLuint vao, GLsizei available,
                    GLint first, GLsizei count, int width, int height) noexcept;
bool draw_triangle(const GlApi& gl, GLuint program, GLuint vao,
                   int width, int height) noexcept;
} // namespace study_gl
