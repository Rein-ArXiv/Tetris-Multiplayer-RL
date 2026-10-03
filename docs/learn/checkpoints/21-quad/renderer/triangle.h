#pragma once
// One-shot teaching triangle draws (exclusive pass).
// The detailed three-vertex contract below describes draw_triangle.
// draw_triangles generalizes the range and caller-selected culling as noted
// at its declaration; the other pass and lifetime requirements still apply.
//
// draw_triangle is non-owning and single-purpose. The caller keeps ownership
// of every GL object. CPU source vertices may already be gone after upload.
// Nothing here creates,
// deletes, allocates, or uploads, and no buffer is swapped.
//
// Preconditions, all the caller's responsibility:
//   * The GlApi was loaded successfully and a desktop OpenGL 3.3 Core context
//     is current on this thread.
//   * The default framebuffer 0 is bound and its state is "fresh": no scissor
//     or depth/stencil test, no blending, no face culling, no rasterizer
//     discard, polygon mode FILL, RGBA writes enabled, no custom framebuffer.
//   * `vao` already records a buffer holding exactly three study_mesh::Vertex2
//     values with attribute location 0 configured as vec2.
//   * `program` is a linked vertex+fragment program whose vertex shader reads
//     attribute location 0 as vec2.
//
// State policy, stated explicitly: this is a simple teaching pass, not a
// universal state-restoring renderer. The previously bound program and vertex
// array are NOT queried and NOT restored. On the way out both are unbound
// (vertex array 0, then program 0) as an explicit pass boundary. Failed GL calls may prevent cleanup;
// false never certifies complete state restoration. Viewport and clear color are left as written.
//
// The function draws into a `width` x `height` drawable: it sets the viewport,
// clears the color buffer to a fixed dark blue, then submits the three
// vertices once with DrawArrays(Triangles, 0, 3).
//
// Rejection happens before any GL mutation when: width <= 0 or height <= 0;
// program == 0 or vao == 0; or gl.GetError() reports a pre-existing error (a
// dirty error state would make every later per-call check ambiguous).
//
// After each of its own GL calls the function checks gl.GetError(). On failure
// it prints a diagnostic naming the failing stage to stderr and returns false.
// Cleanup still unbinds both objects, but cleanup errors are not collected on
// those failure paths. It returns true only when every call, including the
// final checked unbind on the success path, reports no error.

#include "renderer/gl_api.h"  // study_gl::GlApi, GLuint

namespace study_gl {

// Same exclusive frame pass as draw_triangle, with a checked vertex range.
// available must be the uploaded count of the buffer referenced by vao.
// This metadata is caller-supplied: it does NOT inspect or authenticate VAO state.
// Caller selects the face-culling policy; this function does not change it.
// first/count are vertex units. Reject nonpositive count, incomplete groups,
// negative first, and out-of-range extent before any GL access.
// Clears each call: to accumulate multiple draws, a later pass API must split
// frame preparation from submission. Here a quad uses one draw of six vertices.
bool draw_triangles(const GlApi& gl, GLuint program, GLuint vao, GLsizei available,
                    GLint first, GLsizei count, int width, int height) noexcept;


// Draws the three vertices already uploaded into `vao`, once, as triangles,
// after clearing the color attachment (not restricted by viewport).
bool draw_triangle(const GlApi& gl, GLuint program, GLuint vao,
                   int width, int height) noexcept;

} // namespace study_gl
