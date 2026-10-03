#include "renderer/program.h"

#include <cstddef>
#include <cstdio>
#include <vector>

namespace study_gl {
namespace {

// Records the current GL error, if any, as an "API failure" diagnostic and
// reports it on stderr. glGetError is a context-wide status flag rather than a
// per-call exception. Read one flag at each boundary; this is not an exhaustive
// diagnostic collector. Returns true when the stage is clean.
bool api_stage(const GlApi& gl, std::string& diagnostic, const char* stage) {
    const GLenum err = gl.GetError();
    if (err == 0) {
        return true;
    }
    char buffer[128];
    std::snprintf(buffer, sizeof buffer,
                  "API failure after %s (GL error 0x%04X)", stage,
                  static_cast<unsigned>(err));
    diagnostic = buffer;
    std::fprintf(stderr, "Program: %s\n", diagnostic.c_str());
    return false;
}

} // namespace

Program::Program(const GlApi& gl) noexcept : gl_(gl) {}

Program::~Program() noexcept { reset(); }

bool Program::link(GLuint vertex, GLuint fragment) {
    // One program per object: never overwrite an owned name. Reject the reuse
    // without touching the existing diagnostic so the caller can still inspect
    // the previous result.
    if (name_ != 0) {
        std::fprintf(stderr, "Program::link: program is not empty\n");
        return false;
    }

    diagnostic_.clear();
    // Linking needs two distinct, already-compiled stages. Reject the obvious
    // caller mistakes before any GL call.
    if (vertex == 0 || fragment == 0) {
        diagnostic_ = "Input failure: shader name is 0";
        return false;
    }
    if (vertex == fragment) {
        diagnostic_ = "Input failure: vertex and fragment names are equal";
        return false;
    }

    // Every failure below releases the program name. std::string and
    // std::vector operations can throw (for example bad_alloc), so the GL
    // section is guarded: an exception still frees the program before it
    // propagates. The borrowed shader names are never deleted here.
    try {
        // Require a clean entry boundary; a pending error would make every
        // later stage check meaningless.
        if (!api_stage(gl_, diagnostic_, "entry (pre-existing error)")) {
            reset();
            return false;
        }

        name_ = gl_.CreateProgram();
        if (!api_stage(gl_, diagnostic_, "glCreateProgram")) {
            reset();
            return false;
        }
        if (name_ == 0) {
            diagnostic_ = "API failure: glCreateProgram returned 0";
            std::fprintf(stderr, "Program: %s\n", diagnostic_.c_str());
            reset();
            return false;
        }

        // Attach both stages before linking. The shader objects stay owned by
        // the caller; successful linking already makes the executable independent of later
        // shader changes. Detachment removes the remaining object references.
        gl_.AttachShader(name_, vertex);
        if (!api_stage(gl_, diagnostic_, "glAttachShader(vertex)")) {
            reset();
            return false;
        }
        gl_.AttachShader(name_, fragment);
        if (!api_stage(gl_, diagnostic_, "glAttachShader(fragment)")) {
            reset();
            return false;
        }

        gl_.LinkProgram(name_);
        if (!api_stage(gl_, diagnostic_, "glLinkProgram")) {
            reset();
            return false;
        }

        GLint status = 0;
        gl_.GetProgramiv(name_, LinkStatus, &status);
        if (!api_stage(gl_, diagnostic_, "glGetProgramiv(GL_LINK_STATUS)")) {
            reset();
            return false;
        }

        // The log is fetched whether or not linking succeeded: on failure it
        // explains why, and on success it may carry warnings.
        GLint log_length = 0;
        gl_.GetProgramiv(name_, InfoLogLength, &log_length);
        if (!api_stage(gl_, diagnostic_, "glGetProgramiv(GL_INFO_LOG_LENGTH)")) {
            reset();
            return false;
        }
        if (log_length < 0) {
            diagnostic_ = "API failure: glGetProgramiv(GL_INFO_LOG_LENGTH) was negative";
            std::fprintf(stderr, "Program: %s\n", diagnostic_.c_str());
            reset();
            return false;
        }

        std::string log;
        if (log_length > 1) {
            // GL_INFO_LOG_LENGTH counts the terminating NUL. Zero-initialize so
            // a driver that writes nothing still leaves readable bytes.
            std::vector<GLchar> buffer(static_cast<std::size_t>(log_length), '\0');
            GLsizei written = 0;
            gl_.GetProgramInfoLog(name_, log_length, &written, buffer.data());
            if (!api_stage(gl_, diagnostic_, "glGetProgramInfoLog")) {
                reset();
                return false;
            }
            // `written` excludes the terminator, so a valid value lies in
            // [0, log_length). Append exactly the bytes the driver reported.
            if (written < 0 || static_cast<GLint>(written) >= log_length) {
                diagnostic_ = "API failure: glGetProgramInfoLog reported an invalid length";
                std::fprintf(stderr, "Program: %s\n", diagnostic_.c_str());
                reset();
                return false;
            }
            log.assign(buffer.data(), static_cast<std::size_t>(written));
        }

        if (status == 0) {
            // A failed link is a link failure even with a clean error flag or an
            // empty log; keep a fallback message in the latter case.
            diagnostic_ = "GLSL link failed";
            if (log.empty()) {
                diagnostic_ += ": no linker log";
            } else {
                diagnostic_ += ": ";
                diagnostic_ += log;
            }
            std::fprintf(stderr, "Program: %s\n", diagnostic_.c_str());
            reset();
            return false;
        }

        // Release the program-to-shader references after successful linking.
        // This does not remove or rebuild the linked executable.
        gl_.DetachShader(name_, vertex);
        if (!api_stage(gl_, diagnostic_, "glDetachShader(vertex)")) {
            reset();
            return false;
        }
        gl_.DetachShader(name_, fragment);
        if (!api_stage(gl_, diagnostic_, "glDetachShader(fragment)")) {
            reset();
            return false;
        }

        // A successful link may still emit warnings; keep them for the caller
        // and report success without binding the program.
        if (!log.empty()) {
            diagnostic_ = "GLSL link log: ";
            diagnostic_ += log;
        }
        return true;
    } catch (...) {
        reset();
        throw;
    }
}

void Program::reset() noexcept {
    if (name_ != 0) {
        gl_.DeleteProgram(name_);
        name_ = 0;
    }
}

} // namespace study_gl
