#pragma once
#include "renderer/color_batch.h"
#include "renderer/program.h"
#include "renderer/shader.h"
#include <limits>
namespace study_batch {
inline constexpr char vertex_shader[]=R"glsl(#version 330 core
layout(location=0) in vec2 a_position;
layout(location=1) in vec4 a_color;
out vec4 v_color;
void main(){gl_Position=vec4(a_position,0,1);v_color=a_color;}
)glsl";
inline constexpr char fragment_shader[]=R"glsl(#version 330 core
in vec4 v_color;
layout(location=0) out vec4 out_color;
void main(){out_color=v_color;}
)glsl";
static_assert(Batch::capacity <= static_cast<std::size_t>(std::numeric_limits<study_gl::GLsizei>::max()));
static_assert(Batch::capacity*sizeof(Vertex) <= static_cast<std::size_t>(std::numeric_limits<study_gl::GLsizeiptr>::max()));

// One opaque pipeline, current GL 3.3 Core, complete table. The table/context
// outlive this owner. Caller owns viewport/scissor/blend and clears the frame.
class Device {
public:
    explicit Device(const study_gl::GlApi& gl) noexcept : gl_(gl),program_(gl) {}
    ~Device() noexcept { reset(); }
    Device(const Device&)=delete;
    Device& operator=(const Device&)=delete;
    Device(Device&&)=delete;
    Device& operator=(Device&&)=delete;
    bool init() {
        using namespace study_gl;
        if (ready_ || buffer_ || array_ || program_.name() || gl_.GetError()) return false;
        Shader vs(gl_),fs(gl_);
        if (!vs.compile(VertexShader,vertex_shader) || !fs.compile(FragmentShader,fragment_shader) ||
            !program_.link(vs.name(),fs.name())) {reset();return false;}
        gl_.GenBuffers(1,&buffer_);
        gl_.GenVertexArrays(1,&array_);
        if (!buffer_ || !array_ || gl_.GetError()) {reset();return false;}
        gl_.BindVertexArray(array_);
        gl_.BindBuffer(ArrayBuffer,buffer_);
        gl_.VertexAttribPointer(0,2,Float,False,sizeof(Vertex),nullptr);
        gl_.EnableVertexAttribArray(0);
        gl_.VertexAttribPointer(1,4,Float,False,sizeof(Vertex),
            reinterpret_cast<const void*>(offsetof(Vertex,r)));
        gl_.EnableVertexAttribArray(1);
        const bool configured=gl_.GetError()==0;
        gl_.BindVertexArray(0);
        gl_.BindBuffer(ArrayBuffer,0);
        if (!configured || gl_.GetError()) {reset();return false;}
        ready_=true;
        return true;
    }
    // Redefine storage with exactly the used prefix, then submit once. Empty
    // batches do no GL work. On a GL failure stop this frame; do not retry it.
    bool submit(const Batch& batch) noexcept {
        using namespace study_gl;
        if (!ready_) return false;
        if (batch.size()==0) return true;
        if (gl_.GetError()) return false;
        gl_.BindBuffer(ArrayBuffer,buffer_);
        bool ok=gl_.GetError()==0;
        if (ok) {
            gl_.BufferData(ArrayBuffer,static_cast<GLsizeiptr>(batch.size()*sizeof(Vertex)),
                           batch.data(),DynamicDraw);
            ok=gl_.GetError()==0;
        }
        if (ok) {
            gl_.UseProgram(program_.name());
            gl_.BindVertexArray(array_);
            ok=gl_.GetError()==0;
            if (ok) {
                gl_.DrawArrays(Triangles,0,static_cast<GLsizei>(batch.size()));
                ok=gl_.GetError()==0;
            }
        }
        gl_.BindVertexArray(0);
        gl_.UseProgram(0);
        gl_.BindBuffer(ArrayBuffer,0);
        const bool cleaned=gl_.GetError()==0;
        return ok && cleaned;
    }
private:
    void reset() noexcept {
        // Device never leaves its program current after submit().
        if (array_)gl_.DeleteVertexArrays(1,&array_);
        if (buffer_)gl_.DeleteBuffers(1,&buffer_);
        array_=buffer_=0;ready_=false;program_.reset();
    }
    const study_gl::GlApi& gl_;
    study_gl::Program program_;
    study_gl::GLuint buffer_=0,array_=0;
    bool ready_=false;
};
} // namespace study_batch
