#include "renderer/program.h"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>
using namespace study_gl;
static void require(bool ok, const char* msg) {
    if (!ok) { std::fprintf(stderr,"contract: %s\n",msg); std::exit(1); }
}
namespace {
struct Fixture {
    int step=0, fail_at=0, deletes=0, shader_deletes=0, uses=0;
    GLenum error=0;
    bool zero=false, linked=true, throws=false;
    int log_length=-2, written=-2; // -2 uses the real text length.
    std::string log;
    std::vector<GLuint> attached, detached;
} f;
void tick() { if (++f.step==f.fail_at) f.error=0x0502; }
GLenum STUDY_GL_CALL error() { auto e=f.error; f.error=0; return e; }
GLuint STUDY_GL_CALL create() { tick(); return f.zero?0:71; }
void STUDY_GL_CALL attach(GLuint p,GLuint s) {
    require(p==71 && (s==11 || s==12),"attach borrowed shaders");
    f.attached.push_back(s); tick();
}
void STUDY_GL_CALL link(GLuint p) { require(p==71,"link name"); tick(); }
void STUDY_GL_CALL query(GLuint p,GLenum what,GLint* out) {
    require(p==71,"query name"); tick();
    if (what==LinkStatus) *out=f.linked;
    else { require(what==InfoLogLength,"query field"); *out=f.log_length==-2?static_cast<GLint>(f.log.size()+1):f.log_length; }
}
void STUDY_GL_CALL log(GLuint p,GLsizei size,GLsizei* written,GLchar* out) {
    require(p==71 && size>1,"log capacity"); tick();
    if(f.throws) throw std::runtime_error("synthetic diagnostic exception");
    const auto n=std::min(f.log.size(),static_cast<std::size_t>(size-1));
    std::memcpy(out,f.log.data(),n); out[n]=0;
    *written=f.written==-2?static_cast<GLsizei>(n):f.written;
}
void STUDY_GL_CALL detach(GLuint p,GLuint s) {
    require(p==71 && (s==11 || s==12),"detach borrowed shaders");
    f.detached.push_back(s); tick();
}
void STUDY_GL_CALL remove(GLuint p) { require(p==71,"delete name"); ++f.deletes; }
void STUDY_GL_CALL remove_shader(GLuint) { ++f.shader_deletes; }
void STUDY_GL_CALL use(GLuint) { ++f.uses; }
GlApi api() {
    GlApi gl; gl.GetError=error; gl.CreateProgram=create; gl.AttachShader=attach;
    gl.LinkProgram=link; gl.GetProgramiv=query; gl.GetProgramInfoLog=log;
    gl.DetachShader=detach; gl.DeleteProgram=remove; gl.DeleteShader=remove_shader; gl.UseProgram=use;
    return gl;
}
void no_borrowed_deletion_or_binding() { require(f.shader_deletes==0 && f.uses==0,"does not own shaders or selection"); }
}
int main() {
    const GlApi gl=api();
    {
        Program p(gl); require(p.link(11,12) && p.name()==71 && p.diagnostic().empty(),"success");
        require(f.attached==std::vector<GLuint>({11,12}) && f.detached==f.attached,"two attaches and detaches");
        const int before=f.step;
        require(!p.link(11,12) && p.name()==71 && f.step==before,"duplicate preserves owner");
        p.reset(); p.reset(); require(f.deletes==1 && p.name()==0,"idempotent reset");
    }
    require(f.deletes==1,"no destructor double delete"); no_borrowed_deletion_or_binding();
    for (const auto pair : {std::pair<GLuint,GLuint>{0,12},{11,0},{11,11}}) {
        f={}; Program p(gl); require(!p.link(pair.first,pair.second) && f.step==0 && !p.diagnostic().empty(),"input rejection");
    }
    f={}; f.zero=true;
    { Program p(gl); require(!p.link(11,12) && f.step==1 && f.deletes==0,"zero creation stops"); }
    f={}; f.error=0x0502;
    { Program p(gl); require(!p.link(11,12) && f.step==0,"dirty entry stops"); }
    // With a nonempty log there are nine GL boundaries: create, two attaches,
    // link, status, length, log, two detaches. Every failure cleans the name.
    for(int fail=1;fail<=9;++fail) {
        f={}; f.fail_at=fail; f.log="note";
        { Program p(gl); require(!p.link(11,12) && p.name()==0 && f.step==fail && f.deletes==1,"API failure short circuit"); }
        no_borrowed_deletion_or_binding();
    }
    for(int length : {0,1}) {
        f={}; f.linked=false; f.log_length=length;
        Program p(gl); require(!p.link(11,12) && p.diagnostic().find("no linker log")!=std::string::npos,"empty failure log");
        require(f.deletes==1 && f.detached.empty(),"failed program deletion releases attachments");
        f.linked=true; require(p.link(11,12),"retry after failure");
    }
    f={}; f.linked=false; f.log="interface mismatch";
    { Program p(gl); require(!p.link(11,12) && p.diagnostic().find(f.log)!=std::string::npos,"link failure log"); }
    f={}; f.log=std::string(6000,'x');
    { Program p(gl); require(p.link(11,12) && p.diagnostic()=="GLSL link log: "+f.log,"complete success log");
      auto old=p.diagnostic(); require(!p.link(11,12) && p.diagnostic()==old,"duplicate preserves diagnostic"); }
    for (int invalid : {-1,5}) {
        f={}; f.log="note"; f.written=invalid;
        Program p(gl); require(!p.link(11,12) && f.deletes==1,"invalid reported length");
    }
    f={}; f.log_length=-1;
    { Program p(gl); require(!p.link(11,12) && f.deletes==1,"negative capacity"); }
    f={}; f.log="note"; f.throws=true;
    { Program p(gl); bool caught=false; try { p.link(11,12); } catch(const std::runtime_error&) { caught=true; }
      require(caught && p.name()==0 && f.deletes==1,"exception cleans and propagates"); }
    no_borrowed_deletion_or_binding();
    std::puts("Program contracts: stages, log, retry, exception, borrowed shaders, selection and ownership passed");
}
