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
#include "renderer/submission.h"
#include "renderer/cpu_timing.h"
#include <chrono>
#include <vector>
#include <cstdio>
#include <exception>
#include <cinttypes>

// The table and borrowed GL strings are used only while the context exists.
static int run_session(study_submission::Mode mode, int interval)
{
    const auto& experiment = study_blend_scene::cases[0];
    const auto swap = platform_set_swap_interval(interval);
    std::printf("swap request=%d attempted=%d accepted=%d reported=%d (not measured refresh)\n",
                interval, swap.attempted, swap.accepted, swap.reported_interval);
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
        const auto vertices = study_blend_scene::make_layers();
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

    double report_elapsed = 0.0;
    unsigned frames = 0;
    unsigned events = 0;
    DrawableSize reported_size{};
    std::vector<study_timing::CpuStages> samples;
    samples.reserve(120);
    while (!platform_should_close() && samples.size() < 120) {
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
        const auto size = platform_drawable_size();
        if (size.width > 0 && size.height > 0) {
            if (size.width != reported_size.width || size.height != reported_size.height) {
                std::printf("%s: drawable %dx%d; continuous window coordinates:\n",
                            experiment.name, size.width, size.height);
                for (const auto& v : study_blend_scene::make_layers()) {
                    const auto clip = study_coordinates::ClipPosition{v.x, v.y, 0.0, 1.0};
                    const auto ndc = study_coordinates::perspective_divide(clip);
                    if (!ndc) return 1;
                    const auto window = study_coordinates::to_window(*ndc, {0,0,size.width,size.height});
                    if (!window) return 1;
                    std::printf("  (%.2f, %.2f), depth %.2f, point-inside=%d\n",
                                window->x, window->y, window->depth,
                                study_coordinates::inside_clip_volume(clip));
                }
                reported_size = size;
            }
            // Use integer nanosecond differences relative to this frame, never
            // convert an absolute epoch to double before subtracting.
            using Clock = std::chrono::steady_clock;
            const auto origin = Clock::now();
            const auto ticks = [&origin]() -> std::uint64_t {
                return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(
                    Clock::now() - origin).count());
            };
            if (!study_blend_scene::render(gl, program.name(), vertex_array.name(),
                                          vertex_buffer.vertex_count(), experiment, size.width, size.height)) return 1;
            const auto after_submit = ticks();
            if (!study_submission::boundary(gl, mode)) return 1;
            const auto after_sync = ticks();
            if (platform_present()) {
                const auto after_present = ticks();
                const auto sample = study_timing::summarize(0, after_submit, after_sync,
                                                           after_present, 1000000000);
                if (!sample) return 1;
                samples.push_back(*sample);
            } // Still reach end_frame when a minimized drawable skips dispatch.
        }
        platform_end_frame();
    }
    // Print outside measured regions; first-use and steady-state samples differ.
    for (std::size_t i = 0; i < samples.size(); ++i) {
        if (i != 0 && i != 59 && i+1 != samples.size()) continue;
        const auto& sample = samples[i];
        std::printf("CPU elapsed frame %zu: submit=%.3fms sync=%.3fms swap-call=%.3fms\n",
                    i+1,sample.submit_ms,sample.sync_ms,sample.present_ms);
    }
    std::puts("CPU call elapsed times only: not isolated GPU times or display timestamps.");
    return 0;
}

int main(int argc, char** argv)
{
    study_submission::Mode mode;
    const std::string_view name = argc > 1 ? argv[1] : "submit";
    const std::string_view interval_text = argc > 2 ? argv[2] : "1";
    if (argc > 3 || !study_submission::parse(name,mode) ||
        (interval_text != "0" && interval_text != "1")) {
        std::fputs("Usage: presentation_demo [submit|flush|finish] [0|1]\n", stderr);
        return 2;
    }
    const int interval = interval_text == "1" ? 1 : 0;
    if (!platform_init(640, 480, "Tetris study: presentation")) return 1;
    int result = 1;
    try { result = run_session(mode, interval); }
    catch (const std::exception& e) { std::fprintf(stderr,"Session exception: %s\n",e.what()); }
    platform_shutdown(); // Failure in loading also reaches this cleanup.
    return result;
}
