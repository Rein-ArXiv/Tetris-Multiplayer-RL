#pragma once
#include "quad.h"
#include "triangle.h"
#include <array>
#include <string_view>
namespace study_blend_scene {
struct Case {
    const char* name;
    bool reverse, premultiplied, wrong_factor, disabled, legacy_alpha;
};
inline constexpr Case cases[] = {
    {"ab",false,false,false,false,false},
    {"ba",true,false,false,false,false},
    {"premul",false,true,false,false,false},
    {"double-alpha",false,true,true,false,false},
    {"replace",false,false,false,true,false},
    {"legacy-alpha",false,false,false,false,true}
};
inline const Case* find_case(std::string_view name) noexcept {
    for (const auto& c : cases) if (name == c.name) return &c;
    return nullptr;
}
inline std::array<study_mesh::Vertex2,12> make_layers() noexcept {
    std::array<study_mesh::Vertex2,12> vertices{};
    const auto quad = study_quad::make_quad();
    for (std::size_t i=0; i<6; ++i) {
        vertices[i] = {quad[i].x - 0.25f, quad[i].y};
        vertices[i+6] = {quad[i].x + 0.25f, quad[i].y};
    }
    return vertices;
}
inline constexpr char vertex[] = R"glsl(#version 330 core
layout(location = 0) in vec2 a_position;
flat out vec4 v_color;
void main() {
    v_color = gl_VertexID < 6 ? vec4(1,0,0,0.5) : vec4(0,0,1,0.5);
    gl_Position = vec4(a_position,0,1);
}
)glsl";
inline constexpr char straight_fragment[] = R"glsl(#version 330 core
flat in vec4 v_color;
layout(location = 0) out vec4 out_color;
void main() { out_color = v_color; }
)glsl";
inline constexpr char premul_fragment[] = R"glsl(#version 330 core
flat in vec4 v_color;
layout(location = 0) out vec4 out_color;
void main() { out_color = vec4(v_color.rgb * v_color.a, v_color.a); }
)glsl";
// Explicit exclusive-pass blend state. The two negative cases intentionally
// violate a source-over convention; they demonstrate the visible/alpha error.
inline bool configure(const study_gl::GlApi& gl, const Case& c) noexcept {
    using namespace study_gl;
    if (gl.GetError()) return false;
    gl.Disable(CullFaceCap);
    gl.Disable(FramebufferSrgb); // Numeric channel experiment, not sRGB encoding.
    gl.Disable(Dither);         // Make 8-bit readback comparison easier.
    gl.BlendEquation(FuncAdd);
    const GLenum rgb_source = c.premultiplied && !c.wrong_factor ? One : SrcAlpha;
    gl.BlendFuncSeparate(rgb_source, OneMinusSrcAlpha,
                         c.legacy_alpha ? SrcAlpha : One, OneMinusSrcAlpha);
    if (c.disabled) gl.Disable(Blend); else gl.Enable(Blend);
    return gl.GetError() == 0;
}
inline bool render(const study_gl::GlApi& gl, study_gl::GLuint program,
                   study_gl::GLuint vao, study_gl::GLsizei available,
                   const Case& c, int width, int height, double background_alpha=1.0) noexcept {
    // Validate the entire scene before clearing or submitting the first half.
    if (available != 12 || !program || !vao) return false;
    if (!study_gl::begin_color_frame(gl,width,height,{0,0,0,background_alpha})) return false;
    const int first = c.reverse ? 6 : 0;
    return study_gl::submit_triangles(gl,program,vao,available,first,6) &&
           study_gl::submit_triangles(gl,program,vao,available,6-first,6);
}
} // namespace study_blend_scene
