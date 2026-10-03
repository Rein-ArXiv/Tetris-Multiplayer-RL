#pragma once
#include "board_geometry.h"
#include "letterbox_scene.h"
#include "program.h"
#include "shader.h"
#include <cstdio>
namespace study_board_scene {
inline constexpr study_letterbox::Size logical{320,240};
static_assert(logical.width == study_board::logical_width &&
              logical.height == study_board::logical_height);
inline constexpr char vertex[] = R"glsl(#version 330 core
layout(location = 0) in vec2 a_position;
void main() { gl_Position = vec4(a_position,0,1); }
)glsl";
inline constexpr char empty_fragment[] = R"glsl(#version 330 core
layout(location = 0) out vec4 out_color;
void main() { out_color = vec4(0.125,0.25,0.375,1); }
)glsl";
inline constexpr char filled_fragment[] = R"glsl(#version 330 core
layout(location = 0) out vec4 out_color;
void main() { out_color = vec4(1,0.5,0,1); }
)glsl";
// Program and context outlive the shader owners used only for this link.
inline bool link_program(const study_gl::GlApi& gl, study_gl::Program& program,
                         const char* fragment_source) {
    study_gl::Shader vs(gl),fs(gl);
    if (!vs.compile(study_gl::VertexShader,vertex) ||
        !fs.compile(study_gl::FragmentShader,fragment_source) ||
        !program.link(vs.name(),fs.name())) {
        std::fprintf(stderr,"board shader/link: %s | %s | %s\n",
                     vs.diagnostic().c_str(),fs.diagnostic().c_str(),program.diagnostic().c_str());
        return false;
    }
    return true;
}
// Exclusive opaque pass, current GL3.3 Core/default framebuffer, FILL mode,
// default write masks and depth/stencil/rasterizer-discard disabled.
// This helper is not a general state save/restore boundary.
inline bool configure(const study_gl::GlApi& gl) noexcept {
    using namespace study_gl;
    if(gl.GetError())return false;
    gl.Disable(Blend);
    gl.Disable(CullFaceCap);
    gl.Disable(FramebufferSrgb);
    gl.Disable(Dither);
    return gl.GetError()==0;
}
inline bool render(const study_gl::GlApi& gl,study_gl::GLuint empty_program,
                   study_gl::GLuint filled_program,study_gl::GLuint vao,
                   study_gl::GLsizei available,std::size_t empty_vertices,
                   const study_letterbox::Layout& layout) noexcept {
    using namespace study_gl;
    // Validate metadata before clearing. It must match the uploaded Mesh.
    if(!empty_program || !filled_program || !vao ||
       available!=static_cast<GLsizei>(study_board::vertex_count) ||
       empty_vertices>study_board::vertex_count || empty_vertices%6!=0)return false;
    if(!study_letterbox_scene::begin(gl,layout,{0.03125,0.0625,0.09375,1}))return false;
    const auto empty=static_cast<GLsizei>(empty_vertices);
    const auto filled=available-empty;
    // submit_triangles intentionally rejects count==0; skip an absent group.
    if(empty>0 && !submit_triangles(gl,empty_program,vao,available,0,empty))return false;
    if(filled>0 && !submit_triangles(gl,filled_program,vao,available,empty,filled))return false;
    return true;
}
} // namespace study_board_scene
