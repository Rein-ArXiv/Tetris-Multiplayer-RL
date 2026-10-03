#pragma once
#include "gl_api.h"
#include <string_view>
namespace study_submission {
enum class Mode { submit, flush, finish };
inline bool parse(std::string_view text, Mode& result) noexcept {
    if (text == "submit") result = Mode::submit;
    else if (text == "flush") result = Mode::flush;
    else if (text == "finish") result = Mode::finish;
    else return false;
    return true;
}
// After drawing, choose an observation boundary. No mode presents a window.
// Flush initiates progress, Finish waits for prior GL effects; neither proves
// compositor/scanout completion. Diagnostic Finish reduces normal CPU/GPU overlap.
inline bool boundary(const study_gl::GlApi& gl, Mode mode) noexcept {
    if (mode != Mode::submit && mode != Mode::flush && mode != Mode::finish) return false;
    if (gl.GetError()) return false;
    switch (mode) {
    case Mode::submit: break;
    case Mode::flush: gl.Flush(); break;
    case Mode::finish: gl.Finish(); break;
    }
    return gl.GetError() == 0;
}
} // namespace study_submission
