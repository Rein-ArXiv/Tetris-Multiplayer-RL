#include "platform/platform.h"
#include "renderer/gl_api.h"
#include "renderer/mesh.h"
#include "renderer/vertex_buffer.h"
#include "renderer/vertex_array.h"
#include "renderer/shader.h"
#include "renderer/program.h"
#include "renderer/triangle.h"
#include "renderer/shader_sources.h"
#include "renderer/coordinates.h"
#include "renderer/quad.h"
#include "renderer/blend_scene.h"
#include "renderer/letterbox_scene.h"
#include <cstdio>
#include <exception>

// The table and borrowed GL strings are used only while the context exists.
static int run_session()
{
    const auto& experiment = study_blend_scene::cases[0];
    const auto swap = platform_set_swap_interval(1);
    std::printf("swap request=%d attempted=%d accepted=%d reported=%d (not measured refresh)\n",
                1, swap.attempted, swap.accepted, swap.reported_interval);
    study_gl::GlApi gl;
    if (!study_gl::load(gl, platform_gl_get_proc)) return 1;
    const auto* version = gl.GetString(study_gl::Version);
    const auto* renderer = gl.GetString(study_gl::Renderer);
    study_gl::GLint attributes = 0, major = 0, minor = 0, profile = 0;
    gl.GetIntegerv(study_gl::MajorVersion, &major);
    gl.GetIntegerv(study_gl::MinorVersion, &minor);
    gl.GetIntegerv(study_gl::ContextProfileMask, &profile);
    gl.GetIntegerv(study_gl::MaxVertexAttribs, &attributes);
    const bool version_ok = major > 3 || (major == 3 && minor >= 3);
    if (!version || !renderer || gl.GetError() != 0 || !version_ok ||
        !(profile & study_gl::CoreProfileBit)) {
        std::fputs("GL diagnostic query/version/profile failed\n", stderr);
        return 1;
    }
    std::printf("GL %s | %s | actual=%d.%d core, max attributes=%d\n",
                reinterpret_cast<const char*>(version), reinterpret_cast<const char*>(renderer),
                major, minor, attributes);

    // Declared after `gl`, destroyed before it and before platform_shutdown().
    study_gl::VertexBuffer vertex_buffer(gl);
    {
        const auto vertices = study_letterbox_scene::make_layers();
        if (!vertex_buffer.upload(vertices)) return 1;
    } // CPU array is no longer needed; the GL data store owns a copy.
    study_gl::GLint stored_bytes = 0;
    gl.GetBufferParameteriv(study_gl::ArrayBuffer, study_gl::BufferSize, &stored_bytes);
    if (gl.GetError() != 0 || stored_bytes != 96) return 1;
    std::printf("GL vertex buffer stores %d bytes (ready for draw)\n", stored_bytes);

    // A VAO borrows the buffer: destroy the VAO first, then the buffer.
    study_gl::VertexArray vertex_array(gl);
    if (!vertex_array.configure(vertex_buffer.name())) return 1;
    study_gl::GLint stride = 0, enabled = 0;
    gl.GetVertexAttribiv(0, study_gl::AttribStride, &stride);
    gl.GetVertexAttribiv(0, study_gl::AttribEnabled, &enabled);
    if (gl.GetError() != 0 || stride != sizeof(study_mesh::Vertex2) || enabled != 1) return 1;
    std::puts("VAO location 0: two floats, stride 8, offset 0, enabled (ready for draw)");

    study_gl::Program program(gl);
    { // Shader owners can end after successful linking and detachment.
        study_gl::Shader vertex(gl), fragment(gl);
        if (!vertex.compile(study_gl::VertexShader, study_blend_scene::vertex)) {
            std::fprintf(stderr,"Vertex shader: %s\n",vertex.diagnostic().c_str());
            return 1;
        }
        if (!fragment.compile(study_gl::FragmentShader, experiment.premultiplied ? study_blend_scene::premul_fragment : study_blend_scene::straight_fragment)) {
            std::fprintf(stderr,"Fragment shader: %s\n",fragment.diagnostic().c_str());
            return 1;
        }
        // Successful compilation may still carry a useful warning.
        if (!vertex.diagnostic().empty()) std::fprintf(stderr,"Vertex note: %s\n",vertex.diagnostic().c_str());
        if (!fragment.diagnostic().empty()) std::fprintf(stderr,"Fragment note: %s\n",fragment.diagnostic().c_str());
        if (!program.link(vertex.name(), fragment.name())) {
            std::fprintf(stderr,"Program: %s\n",program.diagnostic().c_str());
            return 1;
        }
    }
    if (!program.diagnostic().empty()) std::fprintf(stderr,"Program note: %s\n",program.diagnostic().c_str());
    // Linking does not select this program. Draw-time selection follows later.
    std::puts("Program linked; temporary shaders released; drawing each visible frame");

    if (!study_blend_scene::configure(gl,experiment)) return 1;

    WindowSize previous_window{};
    DrawableSize previous_drawable{};
    double report_elapsed=0;
    while (!platform_should_close()) {
        const FrameInfo frame=platform_begin_frame();
        if (platform_should_close() || platform_key_pressed(Key::Escape)) break;
        const auto window=platform_window_size();
        const auto drawable=platform_drawable_size();
        const auto layout=study_letterbox::make_layout(
            {window.width,window.height},{drawable.width,drawable.height},
            study_letterbox_scene::logical);
        if (layout) {
            if (window.width!=previous_window.width || window.height!=previous_window.height ||
                drawable.width!=previous_drawable.width || drawable.height!=previous_drawable.height) {
                const auto& v=layout->viewport;
                std::printf("window=%dx%d drawable=%dx%d viewport(top-left)=%d,%d %dx%d GL-y=%d\n",
                    window.width,window.height,drawable.width,drawable.height,
                    v.x,v.y,v.width,v.height,study_letterbox::gl_y(*layout));
                previous_window=window; previous_drawable=drawable;
            }
            const auto mouse=platform_window_mouse();
            const auto point=mouse.available ? study_letterbox::window_to_logical(
                *layout,{static_cast<double>(mouse.x),static_cast<double>(mouse.y)}) : std::nullopt;
            report_elapsed+=frame.dt;
            if (report_elapsed>=0.5) {
                if (point) std::printf("logical mouse=(%.2f,%.2f), red=%d blue=%d\n",point->x,point->y,
                    point->x>=40 && point->x<200 && point->y>=60 && point->y<180,
                    point->x>=120 && point->x<280 && point->y>=60 && point->y<180);
                else std::puts("mouse: outside game viewport or unavailable");
                report_elapsed=0;
            }
            if (!study_letterbox_scene::render(gl,program.name(),vertex_array.name(),
                                               vertex_buffer.vertex_count(),*layout)) return 1;
            (void)platform_present();
        }
        // Invalid/minimized dimensions skip rendering, not frame pacing.
        platform_end_frame();
    }
    return 0;
}

int main()
{
    if (!platform_init(640,480,"Tetris study: resize and letterbox")) return 1;
    int result=1;
    try { result=run_session(); }
    catch (const std::exception& e) { std::fprintf(stderr,"Session exception: %s\n",e.what()); }
    platform_shutdown();
    return result;
}
