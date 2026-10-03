#include "platform/platform.h"
#include "renderer/gl_api.h"
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
    if (!platform_init(640, 480, "Tetris study: GL loader")) return 1;
    const int result = run_session();
    platform_shutdown(); // Failure in loading also reaches this cleanup.
    return result;
}
