#include "simulation/round.h"
#include "presentation/accent_noise.h"
#include "renderer/board_scene.h"
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"CHECK failed at %d: %s\n",__LINE__,#x);std::exit(1);}}while(false)
using Round=study_round::Round;
using Step=study_round::Step;
static void same(const Round& a,const Round& b){
    CHECK(a.board().cells()==b.board().cells() && a.kind()==b.kind() && a.quarter()==b.quarter());
    CHECK(a.end_reason()==b.end_reason()&&a.score()==b.score()&&a.total_lines()==b.total_lines());
    CHECK(a.last_awarded()==b.last_awarded()&&a.last_cleared()==b.last_cleared());
    CHECK(a.pending_garbage()==b.pending_garbage()&&a.last_garbage()==b.last_garbage());
    CHECK(a.attack_sent()==b.attack_sent()&&a.clear_streak()==b.clear_streak());
    CHECK(a.rotation_ready()==b.rotation_ready()&&a.last_t_spin_lines()==b.last_t_spin_lines());
    CHECK(a.gravity().elapsed==b.gravity().elapsed&&a.gravity().interval==b.gravity().interval);
    CHECK(a.active().has_value()==b.active().has_value());
    if(a.active()){
        CHECK(a.active()->origin.row==b.active()->origin.row&&a.active()->origin.column==b.active()->origin.column);
        for(int i=0;i<4;++i)CHECK(a.active()->local[i].row==b.active()->local[i].row&&a.active()->local[i].column==b.active()->local[i].column);
    }
    for(int i=0;i<3;++i)CHECK(a.next().peek(i)==b.next().peek(i));
    const auto *pa=a.source().seeded(),*pb=b.source().seeded();CHECK(pa&&pb);
    CHECK(pa->rng_state()==pb->rng_state()&&pa->bag().remaining()==pb->bag().remaining());
    for(int i=0;i<7;++i)CHECK(pa->bag().at(i)==pb->bag().at(i));
    CHECK(a.hole_source().rng_state()==b.hole_source().rng_state()&&a.hole_cursor()==b.hole_cursor());
}
static void check_bottom(const Round& round,int rows,unsigned hole){
    for(int r=20-rows;r<20;++r)for(int c=0;c<10;++c)
        CHECK(round.board().get(r,c)==(c==static_cast<int>(hole)?study_grid::Cell::empty:study_grid::Cell::filled));
}
// GL call recorder: verify supplied tint reaches the existing clear path.
namespace spy {
using namespace study_gl;
static std::array<float,4> color{};
static unsigned clears=0,draws=0;
static GLenum STUDY_GL_CALL error(){return 0;}
static void STUDY_GL_CALL integers(GLenum,GLint* values){values[0]=4096;values[1]=4096;}
static void STUDY_GL_CALL cap(GLenum){}
static void STUDY_GL_CALL rect(GLint,GLint,GLsizei,GLsizei){}
static void STUDY_GL_CALL tint(GLfloat r,GLfloat g,GLfloat b,GLfloat a){color={r,g,b,a};}
static void STUDY_GL_CALL clear(GLbitfield){++clears;}
static void STUDY_GL_CALL bind(GLuint){}
static void STUDY_GL_CALL draw(GLenum,GLint,GLsizei){++draws;}
static void check(){
    GlApi gl{};gl.GetError=error;gl.GetIntegerv=integers;gl.Disable=cap;gl.Enable=cap;
    gl.Viewport=rect;gl.Scissor=rect;gl.ClearColor=tint;gl.Clear=clear;
    gl.UseProgram=bind;gl.BindVertexArray=bind;gl.DrawArrays=draw;
    const auto layout=study_letterbox::make_layout({640,480},{640,480},{320,240});CHECK(layout);
    for(unsigned index=0;index<5;++index){
        const study_blend::Rgba background{0.03125,0.0625,0.09375+0.002*index,1};
        CHECK(study_board_scene::render(gl,1,2,3,1200,1200,*layout,background));
        CHECK(std::abs(color[2]-background.b)<0.000001 && color[3]==1);
    }
    const unsigned before=clears;
    CHECK(!study_board_scene::render(gl,1,2,3,1200,1200,*layout,{0,0,std::numeric_limits<double>::quiet_NaN(),1}));
    CHECK(clears==before && draws==5);
}
}
int main(){
    CHECK(study_session::garbage_seed(study_session::garbage_tag)==0);
    CHECK(study_holes::HoleSource::seeded(study_session::garbage_tag).rng_state()==88172645463393265ull);
    study_holes::HoleSource scripted;for(int i=0;i<30;++i){const unsigned holes[]={4,8,1};CHECK(!scripted.rng_state());CHECK(scripted.next()==holes[i%3]);CHECK(scripted.cursor()==static_cast<unsigned>((i+1)%3));}
    for(std::uint64_t seed=0;seed<100;++seed){
        study_next::SeededBagSource pieces(seed),piece_oracle(seed);
        auto holes=study_holes::HoleSource::seeded(seed);
        auto hole_oracle=holes;study_presentation::AccentNoise effect(seed+17);
        for(unsigned step=0;step<1000;++step){
            for(unsigned n=0;n<step%13;++n)CHECK(effect.sample()<5);
            if(step%3)CHECK(pieces.next()==piece_oracle.next());
            else CHECK(holes.next()==hole_oracle.next());
            CHECK(pieces.rng_state()==piece_oracle.rng_state()&&holes.rng_state()==hole_oracle.rng_state());
            auto copy=holes;const auto state=holes.rng_state();(void)copy.next();CHECK(holes.rng_state()==state);
        }
        auto baseline=*Round::create_seeded(study_grid::Grid{},seed);
        std::array<Round,3> variants{baseline,baseline,baseline};
        std::array<study_presentation::AccentNoise,3> effects{study_presentation::AccentNoise(17),study_presentation::AccentNoise(17),study_presentation::AccentNoise(17)};
        const unsigned sample_counts[]={0,1,37};
        for(unsigned lock=0;lock<100&&!baseline.finished();++lock){
            const int incoming=lock%3==0?1:0;
            CHECK(baseline.add_garbage(incoming));
            for(auto& round:variants)CHECK(round.add_garbage(incoming));
            const auto result=baseline.tick(0,false,false,true);
            for(unsigned k=0;k<3;++k){
                for(unsigned sample=0;sample<sample_counts[k];++sample)(void)effects[k].sample();
                CHECK(variants[k].tick(0,false,false,true)==result);same(baseline,variants[k]);
            }
        }
        CHECK(baseline.finished());const auto stopped=baseline;
        CHECK(baseline.tick(0,false,false,true)==Step::stopped);same(baseline,stopped);
        auto grouped=*Round::create_seeded(study_grid::Grid{},seed);
        auto combined=grouped;auto expected=study_holes::HoleSource::seeded(seed);
        CHECK(grouped.add_garbage(1)&&grouped.add_garbage(2)&&combined.add_garbage(3));
        const auto pending=grouped;CHECK(grouped.tick(9,false,false,true)==Step::invalid);same(grouped,pending);
        CHECK(grouped.tick(0,false,false,true)==Step::locked&&combined.tick(0,false,false,true)==Step::locked);
        const unsigned hole=expected.next();same(grouped,combined);check_bottom(grouped,3,hole);
        CHECK(grouped.hole_source().rng_state()==expected.rng_state());
        CHECK(grouped.add_garbage(0));CHECK(grouped.tick(0,false,false,true)==Step::locked);
        CHECK(grouped.hole_source().rng_state()==expected.rng_state());
        CHECK(grouped.add_garbage(2));(void)grouped.tick(0,false,false,true);check_bottom(grouped,2,expected.next());
        CHECK(grouped.hole_source().rng_state()==expected.rng_state());
        auto overflow=*Round::create_seeded(study_grid::Grid{},seed);CHECK(overflow.add_garbage(20));
        auto one_hole=study_holes::HoleSource::seeded(seed);const unsigned last=one_hole.next();
        CHECK(overflow.tick(0,false,false,true)==Step::game_over);check_bottom(overflow,20,last);
        CHECK(overflow.hole_source().rng_state()==one_hole.rng_state());
        const auto finished=overflow;CHECK(overflow.tick(0)==Step::stopped);same(overflow,finished);
        study_grid::Grid blocked;for(int c=3;c<=6;++c){CHECK(blocked.set(0,c,study_grid::Cell::filled));CHECK(blocked.set(1,c,study_grid::Cell::filled));}
        auto initial=*Round::create_seeded(blocked,seed);CHECK(initial.finished());
        CHECK(initial.hole_source().rng_state()==study_holes::HoleSource::seeded(seed).rng_state());
    }
    spy::check();
    std::puts("100000 interleaved stream operations; 100 Round histories x3 visual rates; batch merge/zero/invalid/overflow/stopped; XOR-zero engine fallback; scripted compatibility; GL tint pass and invalid-color preservation passed");
}
