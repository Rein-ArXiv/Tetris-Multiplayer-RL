#include "platform/platform.h"
#include "renderer/gl_api.h"
#include "renderer/mesh.h"
#include "renderer/vertex_buffer.h"
#include "renderer/vertex_array.h"
#include <cstdio>
#include <cinttypes>

// The table and borrowed GL strings are used only while the context exists.
static int run_session()
{
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
        const auto vertices = study_mesh::make_triangle();
        if (!vertex_buffer.upload(vertices)) return 1;
    } // CPU array is no longer needed; the GL data store owns a copy.
    study_gl::GLint stored_bytes = 0;
    gl.GetBufferParameteriv(study_gl::ArrayBuffer, study_gl::BufferSize, &stored_bytes);
    if (gl.GetError() != 0 || stored_bytes != 24) return 1;
    std::printf("GL vertex buffer stores %d bytes (not drawn)\n", stored_bytes);

    // A VAO borrows the buffer: destroy the VAO first, then the buffer.
    study_gl::VertexArray vertex_array(gl);
    if (!vertex_array.configure(vertex_buffer.name())) return 1;
    study_gl::GLint stride = 0, enabled = 0;
    gl.GetVertexAttribiv(0, study_gl::AttribStride, &stride);
    gl.GetVertexAttribiv(0, study_gl::AttribEnabled, &enabled);
    if (gl.GetError() != 0 || stride != sizeof(study_mesh::Vertex2) || enabled != 1) return 1;
    std::puts("VAO location 0: two floats, stride 8, offset 0, enabled (not drawn)");

    double report_elapsed = 0.0;
    unsigned frames = 0;
    unsigned events = 0;
    while (!platform_should_close()) {
        const FrameInfo frame = platform_begin_frame();
        if (platform_should_close()) break;
        if (platform_key_pressed(Key::Escape)) break;
        if (platform_key_pressed(Key::Left)) std::puts("left pressed");
        if (platform_key_released(Key::Left)) std::puts("left released");
        while (const char ch = platform_get_char_pressed())
            std::printf("text byte=%u\n", static_cast<unsigned>(static_cast<unsigned char>(ch)));
        ++frames;
        events += frame.events;
        report_elapsed += frame.dt;
        if (report_elapsed >= 1.0) {
            std::printf("text dropped=%" PRIu64 "\n", platform_text_dropped());
            std::printf("left held=%d right held=%d\n",
                        platform_key_down(Key::Left), platform_key_down(Key::Right));
            std::printf("elapsed=%.3f frames=%u events=%u last_dt=%.6f\n",
                        report_elapsed, frames, events, frame.dt);
            report_elapsed = 0.0;
            frames = events = 0;
        }
        platform_end_frame();
    }
    return 0;
}

int main()
{
    if (!platform_init(640, 480, "Tetris study: VAO")) return 1;
    const int result = run_session();
    platform_shutdown(); // Failure in loading also reaches this cleanup.
    return result;
}
