#pragma once
#include "renderer/texture.h"
#include "renderer/program.h"
#include "renderer/shader.h"
#include <cstddef>
#include <type_traits>
namespace study_texture {
struct QuadVertex { float x,y,u,v; };
static_assert(std::is_standard_layout_v<QuadVertex> && sizeof(QuadVertex)==16);
static_assert(offsetof(QuadVertex,u)==8);
inline constexpr char vertex_source[]=R"glsl(#version 330 core
layout(location=0) in vec2 a_position;
layout(location=1) in vec2 a_uv;
out vec2 v_uv;
void main(){gl_Position=vec4(a_position,0,1);v_uv=a_uv;}
)glsl";
inline constexpr char fragment_source[]=R"glsl(#version 330 core
in vec2 v_uv;
uniform sampler2D u_image;
layout(location=0) out vec4 out_color;
void main(){out_color=texture(u_image,v_uv);}
)glsl";
// Fixed 48x48 logical badge at (20,20) in the 320x240 scene. Row 0 is top.
// Own pipeline: texture unit 0 has no sampler object; blend/depth are disabled.
class Quad {
public:
    explicit Quad(const study_gl::GlApi& gl) noexcept : gl_(gl),program_(gl) {}
    ~Quad() noexcept { reset(); }
    Quad(const Quad&)=delete;
    Quad& operator=(const Quad&)=delete;
    bool init() {
        using namespace study_gl;
        if (ready_ || gl_.GetError()) return false;
        Shader vs(gl_),fs(gl_);
        if(!vs.compile(VertexShader,vertex_source)||!fs.compile(FragmentShader,fragment_source)||
           !program_.link(vs.name(),fs.name())) {reset();return false;}
        location_=gl_.GetUniformLocation(program_.name(),"u_image");
        constexpr float l=20.f/160.f-1.f,r=68.f/160.f-1.f;
        constexpr float t=1.f-20.f/120.f,b=1.f-68.f/120.f;
        constexpr QuadVertex vertices[]={{l,t,0,0},{l,b,0,1},{r,b,1,1},
                                          {l,t,0,0},{r,b,1,1},{r,t,1,0}};
        gl_.GenBuffers(1,&buffer_);gl_.GenVertexArrays(1,&array_);
        if(location_<0 || !buffer_ || !array_ || gl_.GetError()){reset();return false;}
        gl_.BindVertexArray(array_);
        bool ok=gl_.GetError()==0;
        if(ok){gl_.BindBuffer(ArrayBuffer,buffer_);ok=gl_.GetError()==0;}
        if(ok){gl_.BufferData(ArrayBuffer,sizeof(vertices),vertices,StaticDraw);ok=gl_.GetError()==0;}
        if(ok){
            gl_.VertexAttribPointer(0,2,Float,False,sizeof(QuadVertex),nullptr);
            gl_.EnableVertexAttribArray(0);
            gl_.VertexAttribPointer(1,2,Float,False,sizeof(QuadVertex),
                                    reinterpret_cast<const void*>(offsetof(QuadVertex,u)));
            gl_.EnableVertexAttribArray(1);
            ok=gl_.GetError()==0;
        }
        gl_.BindVertexArray(0);gl_.BindBuffer(ArrayBuffer,0);
        const bool clean=gl_.GetError()==0;
        if(!ok || !clean){reset();return false;}
        ready_=true;return true;
    }
    bool draw(const Texture& image) noexcept {
        using namespace study_gl;
        if(!ready_ || !image.name() || gl_.GetError())return false;
        // The game owns unit 0 and leaves it clear between passes.
        gl_.ActiveTexture(Texture0);gl_.BindTexture(Texture2D,image.name());
        gl_.UseProgram(program_.name());gl_.Uniform1i(location_,0);
        gl_.BindVertexArray(array_);
        bool ok=gl_.GetError()==0;
        if(ok){gl_.DrawArrays(Triangles,0,6);ok=gl_.GetError()==0;}
        gl_.BindVertexArray(0);gl_.UseProgram(0);gl_.BindTexture(Texture2D,0);
        const bool clean=gl_.GetError()==0;
        return ok && clean;
    }
private:
    void reset() noexcept {
        if(array_)gl_.DeleteVertexArrays(1,&array_);
        if(buffer_)gl_.DeleteBuffers(1,&buffer_);
        array_=buffer_=0;ready_=false;program_.reset();
    }
    const study_gl::GlApi& gl_;
    study_gl::Program program_;
    study_gl::GLuint buffer_=0,array_=0;
    study_gl::GLint location_=-1;
    bool ready_=false;
};
} // namespace study_texture
