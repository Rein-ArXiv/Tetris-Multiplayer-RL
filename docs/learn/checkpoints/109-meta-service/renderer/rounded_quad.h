#pragma once
#include "renderer/rounded_geometry.h"
#include "renderer/texture.h"
#include "renderer/program.h"
#include "renderer/shader.h"
namespace study_rounded {
inline constexpr char vertex_shader[]=R"glsl(#version 330 core
layout(location=0) in vec2 a_position;
layout(location=1) in vec2 a_uv;
layout(location=2) in vec4 a_tint;
layout(location=3) in vec2 a_local;
layout(location=4) in vec2 a_half;
layout(location=5) in float a_radius;
out vec2 v_uv;
out vec4 v_tint;
out vec2 v_local;
flat out vec2 v_half;
flat out float v_radius;
void main(){gl_Position=vec4(a_position,0,1);v_uv=a_uv;v_tint=a_tint;v_local=a_local;v_half=a_half;v_radius=a_radius;}
)glsl";
inline constexpr char fragment_shader[]=R"glsl(#version 330 core
in vec2 v_uv;
in vec4 v_tint;
in vec2 v_local;
flat in vec2 v_half;
flat in float v_radius;
uniform sampler2D u_image;
layout(location=0) out vec4 out_color;
float rounded_box_sdf(vec2 p,vec2 b,float r){
    vec2 q=abs(p)-b+vec2(r);
    return length(max(q,0.0))+min(max(q.x,q.y),0.0)-r;
}
void main(){
    vec4 color=texture(u_image,v_uv)*v_tint;
    if(v_radius>0.0){
        float d=rounded_box_sdf(v_local,v_half,v_radius);
        color.a*=1.0-smoothstep(-0.5,0.5,d);
    }
    if(color.a<=0.0)discard;
    out_color=color;
}
)glsl";
// Exclusive image pass: unit 0 without sampler objects; caller sets viewport/clip,
// disables depth/stencil/cull/dither/framebuffer-sRGB and uses default write masks.
// Normal exit clears VAO/VBO/program/unit0 binding and disables blend. This is
// a known pass boundary, not arbitrary caller-state preservation.
class RoundedQuad {
public:
    explicit RoundedQuad(const study_gl::GlApi& gl) noexcept : gl_(gl),program_(gl) {}
    ~RoundedQuad() noexcept { reset(); }
    RoundedQuad(const RoundedQuad&)=delete;
    RoundedQuad& operator=(const RoundedQuad&)=delete;
    bool init() {
        using namespace study_gl;
        if(ready_ || gl_.GetError()) return false;
        Shader vs(gl_),fs(gl_);
        if(!vs.compile(VertexShader,vertex_shader)||!fs.compile(FragmentShader,fragment_shader)||
           !program_.link(vs.name(),fs.name())) {reset();return false;}
        location_=gl_.GetUniformLocation(program_.name(),"u_image");
        gl_.GenBuffers(1,&buffer_);gl_.GenVertexArrays(1,&array_);
        if(location_<0 || !buffer_ || !array_ || gl_.GetError()){reset();return false;}
        gl_.BindVertexArray(array_);bool ok=gl_.GetError()==0;
        if(ok){gl_.BindBuffer(ArrayBuffer,buffer_);ok=gl_.GetError()==0;}
        if(ok){gl_.BufferData(ArrayBuffer,6*sizeof(Vertex),nullptr,DynamicDraw);ok=gl_.GetError()==0;}
        if(ok){
            gl_.VertexAttribPointer(0,2,Float,False,sizeof(Vertex),nullptr);
            gl_.EnableVertexAttribArray(0);
            gl_.VertexAttribPointer(1,2,Float,False,sizeof(Vertex),reinterpret_cast<const void*>(offsetof(Vertex,u)));
            gl_.EnableVertexAttribArray(1);
            gl_.VertexAttribPointer(2,4,Float,False,sizeof(Vertex),reinterpret_cast<const void*>(offsetof(Vertex,r)));
            gl_.EnableVertexAttribArray(2);
            gl_.VertexAttribPointer(3,2,Float,False,sizeof(Vertex),reinterpret_cast<const void*>(offsetof(Vertex,local_x)));
            gl_.EnableVertexAttribArray(3);
            gl_.VertexAttribPointer(4,2,Float,False,sizeof(Vertex),reinterpret_cast<const void*>(offsetof(Vertex,half_width)));
            gl_.EnableVertexAttribArray(4);
            gl_.VertexAttribPointer(5,1,Float,False,sizeof(Vertex),reinterpret_cast<const void*>(offsetof(Vertex,radius)));
            gl_.EnableVertexAttribArray(5);
            ok=gl_.GetError()==0;
        }
        gl_.BindVertexArray(0);gl_.BindBuffer(ArrayBuffer,0);
        const bool clean=gl_.GetError()==0;
        if(!ok || !clean){reset();return false;}
        ready_=true;return true;
    }
    bool draw(const study_texture::Texture& image,const Draw& request) noexcept {
        const auto vertices=make_vertices(request);
        if(!vertices || !ready_ || !image.name()) return false;
        using namespace study_gl;
        if(gl_.GetError())return false;
        gl_.BindVertexArray(array_);bool ok=gl_.GetError()==0;
        if(ok){gl_.BindBuffer(ArrayBuffer,buffer_);ok=gl_.GetError()==0;}
        if(ok){gl_.BufferData(ArrayBuffer,sizeof(*vertices),vertices->data(),DynamicDraw);ok=gl_.GetError()==0;}
        if(ok){
            gl_.ActiveTexture(Texture0);gl_.BindTexture(Texture2D,image.name());
            gl_.UseProgram(program_.name());gl_.Uniform1i(location_,0);
            gl_.BlendEquation(FuncAdd);
            gl_.BlendFuncSeparate(SrcAlpha,OneMinusSrcAlpha,One,OneMinusSrcAlpha);
            gl_.Enable(Blend);ok=gl_.GetError()==0;
        }
        if(ok){gl_.DrawArrays(Triangles,0,6);ok=gl_.GetError()==0;}
        gl_.BindVertexArray(0);gl_.BindBuffer(ArrayBuffer,0);
        gl_.UseProgram(0);gl_.ActiveTexture(Texture0);gl_.BindTexture(Texture2D,0);gl_.Disable(Blend);
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
} // namespace study_rounded
