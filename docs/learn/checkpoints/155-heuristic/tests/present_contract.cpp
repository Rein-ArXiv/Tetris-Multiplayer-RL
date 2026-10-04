#include "renderer/cpu_timing.h"
#include "renderer/submission.h"
#include <climits>
#include <limits>
#include <cstdio>
#include <cstdlib>
using study_submission::Mode;
#define CHECK(x) do {if(!(x)){std::fprintf(stderr,"present line %d\n",__LINE__);std::exit(1);}}while(0)
static int flushes=0,finishes=0;static bool fail=false;static study_gl::GLenum error=0;
static study_gl::GLenum STUDY_GL_CALL get_error(){auto e=error;error=0;return e;}
static void STUDY_GL_CALL flush(){++flushes;if(fail)error=0x0502;}
static void STUDY_GL_CALL finish(){++finishes;if(fail)error=0x0502;}
int main(){
    const auto max=std::numeric_limits<std::uint64_t>::max();
    const auto near_max=study_timing::summarize(max-6,max-5,max-3,max,1000);
    CHECK(near_max && near_max->submit_ms==1 && near_max->sync_ms==2 && near_max->present_ms==3);
    const auto zero=study_timing::summarize(7,7,7,7,1000);
    CHECK(zero && zero->submit_ms==0 && zero->sync_ms==0 && zero->present_ms==0);
    CHECK(!study_timing::summarize(0,1,2,3,0));
    CHECK(!study_timing::summarize(2,1,3,4,1));
    CHECK(!study_timing::summarize(0,2,1,3,1));
    CHECK(!study_timing::summarize(0,1,3,2,1));
    study_gl::GlApi gl;gl.GetError=get_error;gl.Flush=flush;gl.Finish=finish;
    Mode mode=Mode::submit;
    CHECK(study_submission::parse("finish",mode)&&mode==Mode::finish);
    CHECK(!study_submission::parse("unknown",mode)&&mode==Mode::finish);
    CHECK(study_submission::boundary(gl,Mode::submit)&&flushes==0&&finishes==0);
    CHECK(study_submission::boundary(gl,Mode::flush)&&flushes==1&&finishes==0);
    CHECK(study_submission::boundary(gl,Mode::finish)&&flushes==1&&finishes==1);
    CHECK(!study_submission::boundary(gl,static_cast<Mode>(99))&&flushes==1&&finishes==1);
    error=0x0502;CHECK(!study_submission::boundary(gl,Mode::finish)&&finishes==1);
    fail=true;CHECK(!study_submission::boundary(gl,Mode::flush));CHECK(!study_submission::boundary(gl,Mode::finish));
    std::puts("CPU stage arithmetic, mode dispatch and error boundaries passed (no GPU/display claim)");
}
