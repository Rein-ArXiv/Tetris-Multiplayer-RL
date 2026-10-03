#pragma once
#include "letterbox.h"
#include "coordinates.h"
#include "blend_scene.h"
namespace study_letterbox_scene {
inline constexpr study_letterbox::Size logical{320,240};
// Logical top-left corners: red (40,60)-(200,180), blue (120,60)-(280,180).
// CCW after the UI y flip. No VBO rewrite is needed when the window resizes.
inline std::array<study_mesh::Vertex2,12> make_layers() noexcept {
    const double points[12][2] = {
        {40,180},{200,180},{200,60},{40,180},{200,60},{40,60},
        {120,180},{280,180},{280,60},{120,180},{280,60},{120,60}
    };
    std::array<study_mesh::Vertex2,12> vertices{};
    for (std::size_t i=0; i<12; ++i) {
        const auto c = study_coordinates::ui_to_clip(points[i][0],points[i][1],320,240);
        vertices[i] = {static_cast<float>(c->x), static_cast<float>(c->y)};
    }
    return vertices;
}
// Exclusive default-framebuffer pass: current GL3.3 Core context, default
// write masks and depth/stencil disabled. Layout must come from make_layout.
// Leaves viewport/scissor configured; does not restore somebody else's state.
inline bool begin(const study_gl::GlApi& gl, const study_letterbox::Layout& l,
                  study_blend::Rgba background={0,0.25,0,1}) noexcept {
    using namespace study_gl;
    if (!study_blend::valid_rgba(background) || gl.GetError()) return false;
    GLint limits[2]{};
    gl.GetIntegerv(0x0D3A /* GL_MAX_VIEWPORT_DIMS */, limits);
    const auto& v = l.viewport;
    if (gl.GetError() || v.width>limits[0] || v.height>limits[1]) return false;
    gl.Disable(ScissorTest);
    gl.ClearColor(0,0,0,1);
    gl.Clear(ColorBufferBit);
    gl.Viewport(v.x,study_letterbox::gl_y(l),v.width,v.height);
    gl.Scissor(v.x,study_letterbox::gl_y(l),v.width,v.height);
    gl.Enable(ScissorTest);
    gl.ClearColor(static_cast<float>(background.r),static_cast<float>(background.g),
                  static_cast<float>(background.b),static_cast<float>(background.a));
    gl.Clear(ColorBufferBit);
    return gl.GetError()==0;
}
inline bool render(const study_gl::GlApi& gl, study_gl::GLuint program,
                   study_gl::GLuint vao, study_gl::GLsizei available,
                   const study_letterbox::Layout& l) noexcept {
    if (!program || !vao || available!=12) return false;
    return begin(gl,l) &&
        study_gl::submit_triangles(gl,program,vao,available,0,6) &&
        study_gl::submit_triangles(gl,program,vao,available,6,6);
}
} // namespace study_letterbox_scene
