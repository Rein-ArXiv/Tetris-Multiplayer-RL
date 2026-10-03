#include "renderer/shader.h"

#include <cstddef>
#include <cstdio>
#include <limits>
#include <vector>

namespace study_gl {
namespace {

// Records the current GL error, if any, as an "API failure" diagnostic and
// reports it on stderr. glGetError is a context-wide status flag rather than a
// per-call exception. Read one flag at each boundary; this is not an
// exhaustive diagnostic collector. Returns true when the stage is clean.
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
    std::fprintf(stderr, "Shader: %s\n", diagnostic.c_str());
    return false;
}

} // namespace

Shader::Shader(const GlApi& gl) noexcept : gl_(gl) {}

Shader::~Shader() noexcept { reset(); }

bool Shader::compile(GLenum type, std::string_view source) {
    // One stage per object: never overwrite an owned name. Reject the reuse
    // without touching the existing diagnostic so the caller can still inspect
    // the previous result.
    if (name_ != 0) {
        std::fprintf(stderr, "Shader::compile: shader is not empty\n");
        return false;
    }

    diagnostic_.clear();
    // This exercise accepts two stages; GL 3.3 also supports geometry shaders.
    if (type != VertexShader && type != FragmentShader) {
        diagnostic_ = "Input failure: unsupported shader type";
        return false;
    }

    // An empty program is a caller mistake, not a GL condition.
    if (source.empty()) {
        diagnostic_ = "Input failure: source is empty";
        return false;
    }

    // glShaderSource takes a signed GLint length; a longer source cannot be
    // described to the driver, so reject it before any GL call.
    if (source.size() > static_cast<std::size_t>(std::numeric_limits<GLint>::max())) {
        diagnostic_ = "Input failure: source exceeds GLint length";
        return false;
    }

    // Every failure below releases the name. std::string and std::vector
    // operations can throw (for example bad_alloc), so the GL section is
    // guarded: an exception still frees the shader before it propagates.
    try {
        // Require a clean entry boundary; a pending error would make every
        // later stage check meaningless.
        if (!api_stage(gl_, diagnostic_, "entry (pre-existing error)")) {
            reset();
            return false;
        }

        name_ = gl_.CreateShader(type);
        if (!api_stage(gl_, diagnostic_, "glCreateShader")) {
            reset();
            return false;
        }
        if (name_ == 0) {
            diagnostic_ = "API failure: glCreateShader returned 0";
            std::fprintf(stderr, "Shader: %s\n", diagnostic_.c_str());
            reset();
            return false;
        }

        // Pass the exact byte count. Do not rely on a NUL terminator: a
        // string_view is not required to be terminated.
        const GLchar* data = source.data();
        const GLint length = static_cast<GLint>(source.size());
        gl_.ShaderSource(name_, 1, &data, &length);
        if (!api_stage(gl_, diagnostic_, "glShaderSource")) {
            reset();
            return false;
        }

        gl_.CompileShader(name_);
        if (!api_stage(gl_, diagnostic_, "glCompileShader")) {
            reset();
            return false;
        }

        GLint status = 0;
        gl_.GetShaderiv(name_, CompileStatus, &status);
        if (!api_stage(gl_, diagnostic_, "glGetShaderiv(GL_COMPILE_STATUS)")) {
            reset();
            return false;
        }

        // The log is fetched whether or not compilation succeeded: on failure
        // it explains why, and on success it may carry warnings.
        GLint log_length = 0;
        gl_.GetShaderiv(name_, InfoLogLength, &log_length);
        if (!api_stage(gl_, diagnostic_, "glGetShaderiv(GL_INFO_LOG_LENGTH)")) {
            reset();
            return false;
        }
        if (log_length < 0) {
            diagnostic_ = "API failure: glGetShaderiv(GL_INFO_LOG_LENGTH) was negative";
            std::fprintf(stderr, "Shader: %s\n", diagnostic_.c_str());
            reset();
            return false;
        }

        std::string log;
        if (log_length > 1) {
            // GL_INFO_LOG_LENGTH counts the terminating NUL. Zero-initialize
            // so a driver that writes nothing still leaves readable bytes.
            std::vector<GLchar> buffer(static_cast<std::size_t>(log_length), '\0');
            GLsizei written = 0;
            gl_.GetShaderInfoLog(name_, log_length, &written, buffer.data());
            if (!api_stage(gl_, diagnostic_, "glGetShaderInfoLog")) {
                reset();
                return false;
            }
            // `written` excludes the terminator, so a valid value lies in
            // [0, log_length). Append exactly the bytes the driver reported.
            if (written < 0 || static_cast<GLint>(written) >= log_length) {
                diagnostic_ = "API failure: glGetShaderInfoLog reported an invalid length";
                std::fprintf(stderr, "Shader: %s\n", diagnostic_.c_str());
                reset();
                return false;
            }
            log.assign(buffer.data(), static_cast<std::size_t>(written));
        }

        if (status == 0) {
            // A failed compile is a GLSL failure even with a clean error flag
            // or an empty log; keep a fallback message in the latter case.
            diagnostic_ = "GLSL compile failed";
            if (log.empty()) {
                diagnostic_ += ": no compiler log";
            } else {
                diagnostic_ += ": ";
                diagnostic_ += log;
            }
            std::fprintf(stderr, "Shader: %s\n", diagnostic_.c_str());
            reset();
            return false;
        }

        // A successful compile may still emit warnings; keep them for the
        // caller and report success.
        if (!log.empty()) {
            diagnostic_ = "GLSL compile log: ";
            diagnostic_ += log;
        }
        return true;
    } catch (...) {
        reset();
        throw;
    }
}

void Shader::reset() noexcept {
    if (name_ != 0) {
        gl_.DeleteShader(name_);
        name_ = 0;
    }
}

} // namespace study_gl
