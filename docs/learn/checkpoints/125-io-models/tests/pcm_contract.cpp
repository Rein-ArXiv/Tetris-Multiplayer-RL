#include "audio/pcm_s16.h"
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <algorithm>
#define CHECK(e) do{if(!(e)){std::fprintf(stderr,"PCM line %d: %s\n",__LINE__,#e);std::exit(1);}}while(false)
using audio_pcm::layout_s16;
int main() {
    auto l=layout_s16(480,2,48000);
    CHECK(l && l->frames==480 && l->samples==960 && l->bytes==1920 && l->bytes_per_frame==4 && l->seconds==0.01);
    CHECK(!layout_s16(0,2,44100) && !layout_s16(1,0,44100) && !layout_s16(1,3,44100));
    CHECK(!layout_s16(1,1,7999) && !layout_s16(1,1,192001));
    CHECK(layout_s16(1,1,8000) && layout_s16(1,2,192000));
    CHECK(layout_s16(480,2,48000,1920) && !layout_s16(481,2,48000,1920));
    CHECK(!layout_s16(UINT64_MAX,1,44100) && !layout_s16(UINT64_MAX,2,44100));
    CHECK(!layout_s16(1,1,44100,1));
    // Model 32-bit API limits without allocating large arrays.
    for(std::uint32_t channels:{1u,2u}) {
        for(std::size_t budget:{std::size_t(0),std::size_t(65535),std::size_t(INT_MAX),std::size_t(UINT32_MAX),std::size_t(SIZE_MAX)}) {
            auto boundary=budget/(2*channels);
            if(boundary) {auto ok=layout_s16(boundary,channels,44100,budget);CHECK(ok&&ok->bytes<=budget&&ok->samples==boundary*channels);}
            CHECK(!layout_s16(static_cast<std::uint64_t>(boundary)+1,channels,44100,budget));
        }
    }
    using study_audio::Pcm16;
    CHECK(!Pcm16::make(44100,2,{1,2,3}) && !Pcm16::make(44100,1,{}) && !Pcm16::make(44100,0,{1}));
    auto stereo=Pcm16::make(48000,2,{100,-100,200,-200,300,-300});
    CHECK(stereo&&stereo->layout().frames==3&&stereo->at(1,0)==200&&stereo->at(1,1)==-200);
    for(auto pair:{std::pair<std::size_t,std::size_t>{3,0},{0,2},{SIZE_MAX,0},{0,SIZE_MAX}}) {
        bool threw=false;try{(void)stereo->at(pair.first,pair.second);}catch(const std::out_of_range&){threw=true;}CHECK(threw);
    }
    auto copy=*stereo; auto moved=std::move(copy);CHECK(moved.at(2,1)==-300);
    CHECK(copy.layout().samples==copy.samples().size()); // moved-from query is still bounded
    copy=moved;CHECK(copy.at(0,0)==100);
    auto mono=Pcm16::make(8000,1,{7,8,9});CHECK(mono);
    copy=*mono;CHECK(copy.channels()==1&&copy.layout().frames==3&&copy.at(2,0)==9);
    copy=std::move(moved);CHECK(copy.channels()==2&&copy.layout().frames==3&&copy.at(2,1)==-300);
    std::vector<std::int16_t> input={4,5};auto owned=Pcm16::make(8000,1,input);input[0]=999;CHECK(owned&&owned->at(0,0)==4);
    CHECK(Pcm16::make(44100,2,std::vector<std::int16_t>(Pcm16::kTeachingBudgetBytes/2)));
    CHECK(!Pcm16::make(44100,2,std::vector<std::int16_t>(Pcm16::kTeachingBudgetBytes/2+2)));
    const auto tone=study_audio::make_reference_tone();
    CHECK(tone.layout().seconds==1 && tone.layout().frames==44100 && tone.layout().samples==88200 && tone.layout().bytes==176400);
    CHECK(tone.at(0,0)==0&&tone.at(25,0)==6400&&tone.at(50,0)==0&&tone.at(75,0)==-6400&&tone.at(100,0)==0);
    std::int64_t sum=0;
    for(std::size_t f=0;f<44100;++f){auto sample=tone.at(f,0);CHECK(sample==tone.at(f,1)&&sample>=-6400&&sample<=6400);sum+=sample;if(f>=100)CHECK(sample==tone.at(f-100,0));}
    CHECK(sum==0);
    CHECK(*std::min_element(tone.samples().begin(),tone.samples().end())==-6400&&*std::max_element(tone.samples().begin(),tone.samples().end())==6400);
    std::puts("PCM units, limits, interleaving, ownership and 44100 triangle frames passed.");
}
