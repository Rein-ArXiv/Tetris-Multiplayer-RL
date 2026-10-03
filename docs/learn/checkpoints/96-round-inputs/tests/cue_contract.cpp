#include "audio/cue_pcm.h"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#define CHECK(e) do{if(!(e)){std::fprintf(stderr,"line %d: %s\n",__LINE__,#e);std::exit(1);}}while(false)
int main(){
    CHECK(!study_audio::make_cue(static_cast<study_sound::Kind>(-1)));
    CHECK(!study_audio::make_cue(static_cast<study_sound::Kind>(4)));
    const int periods[4]={100,200,64,320};
    std::array<std::vector<std::int16_t>,4> values;
    for(int k=0;k<4;++k){
        auto p=study_audio::make_cue(static_cast<study_sound::Kind>(k));CHECK(p);
        CHECK(p->rate()==44100&&p->channels()==2&&p->layout().frames==4410);
        CHECK(p->at(0,0)==0&&p->at(4409,0)==0);
        values[k]=p->samples();
        for(std::size_t n=0;n<4410;++n){
            CHECK(p->at(n,0)==p->at(n,1));CHECK(std::abs(int(p->at(n,0)))<=2000);
        }
        // Flat-envelope interior must repeat the period and invert at half-period.
        for(std::size_t n=500;n<3000;++n){
            CHECK(p->at(n,0)==p->at(n+periods[k],0));
            CHECK(p->at(n,0)==-p->at(n+periods[k]/2,0));
        }
    }
    for(int i=0;i<4;++i)for(int j=0;j<i;++j)CHECK(values[i]!=values[j]);
    std::puts("Four distinct 100ms stereo cues: envelope, magnitude, period and symmetry passed.");
}
