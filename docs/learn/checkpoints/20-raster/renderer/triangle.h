#pragma once
// Lesson 18: one-shot teaching triangle draw (exclusive pass).
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

// Draws the three vertices already uploaded into `vao`, once, as triangles,
// after clearing the color attachment (not restricted by viewport).
bool draw_triangle(const GlApi& gl, GLuint program, GLuint vao,
                   int width, int height) noexcept;

} // namespace study_gl
