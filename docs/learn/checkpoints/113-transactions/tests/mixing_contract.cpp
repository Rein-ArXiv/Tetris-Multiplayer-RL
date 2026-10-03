#include "audio/mixer.h"
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <vector>
#define CHECK(x) do { if (!(x)) { std::fprintf(stderr,"mix line %d: %s\n",__LINE__,#x); std::exit(1); } } while(false)
using study_audio::Mixer;
using study_audio::Pcm16;

// Independent scalar oracle: wide sum, then a single final range decision.
static int16_t finish(int64_t sum) {
    return static_cast<int16_t>(sum < -32768 ? -32768 : sum > 32767 ? 32767 : sum);
}

static void order_and_bounds() {
    std::array<std::optional<Pcm16>,3> clips{
        Pcm16::make(44100,1,{30000}), Pcm16::make(44100,1,{30000}),
        Pcm16::make(44100,1,{-30000})};
    std::array<int,3> order{0,1,2};
    do {
        Mixer m; CHECK(m.configure(44100,1));
        for (int i=0;i<3;++i) CHECK(m.start(i,*clips[order[i]]));
        int16_t value=0; m.render(&value,2); CHECK(value==30000);
    } while (std::next_permutation(order.begin(),order.end()));
    for (int value : {-32768,32767}) {
        const auto pcm=Pcm16::make(44100,2,std::vector<int16_t>(1200,static_cast<int16_t>(value)));
        CHECK(pcm); Mixer m; CHECK(m.configure(44100,2));
        for (std::size_t slot=0;slot<audio_mix::kMaxVoices;++slot) CHECK(m.start(slot,*pcm));
        CHECK(!m.start(audio_mix::kMaxVoices,*pcm));
        std::array<int16_t,1026> out{}; m.render(out.data(),out.size()*2);
        for (auto n:out) CHECK(n==value);
    }
    for (int n=-32768;n<=32767;++n) {
        CHECK(audio_mix::scaled_sample(static_cast<int16_t>(n),1)==n);
        CHECK(audio_mix::scaled_sample(static_cast<int16_t>(n),0)==0);
        CHECK(audio_mix::scaled_sample(static_cast<int16_t>(n),0.5f)==n/2);
    }
    CHECK(audio_mix::finish_sample(INT32_MAX)==32767);
    CHECK(audio_mix::finish_sample(INT32_MIN)==-32768);
}

static void formats_and_gain() {
    const auto pcm=Pcm16::make(44100,1,{10,20,30});
    const auto wrong=Pcm16::make(48000,1,{99}); CHECK(pcm&&wrong);
    Mixer m; int16_t out=99; m.render(&out,2); CHECK(out==0&&!m.start(0,*pcm));
    CHECK(m.configure(44100,1)&&m.start(0,*pcm,0));
    m.render(&out,2); CHECK(out==0&&m.cursor_frames(0)==1);
    CHECK(!m.start(0,*wrong)); CHECK(!m.configure(0,1));
    CHECK(m.playing(0)&&m.cursor_frames(0)==1);
    CHECK(m.set_gain(0,1)); m.render(&out,2); CHECK(out==20);
    CHECK(!m.set_gain(9,1)&&!m.playing(9)&&m.cursor_frames(9)==0);
    m.stop(99); m.stop_all(); CHECK(!m.playing(0)&&m.cursor_frames(0)==0);
    const float bad[]={std::numeric_limits<float>::quiet_NaN(),
        std::numeric_limits<float>::infinity(),-std::numeric_limits<float>::infinity(),-1};
    for (float gain:bad) {
        CHECK(audio_mix::normalize_gain(gain)==0);
        CHECK(m.start(0,*pcm,gain)); m.render(&out,2);
        CHECK(out==0&&m.cursor_frames(0)==1);
    }
    CHECK(audio_mix::normalize_gain(2)==1&&audio_mix::normalize_gain(0.5f)==0.5f);
    m.render(nullptr,0);
}

static void block_matrix() {
    for (unsigned channels:{1u,2u}) {
        std::array<std::optional<Pcm16>,9> clips;
        std::array<float,9> gains{};
        for (std::size_t v=0;v<clips.size();++v) {
            const std::size_t frames=301+19*v;
            std::vector<int16_t> values(frames*channels);
            for (std::size_t i=0;i<values.size();++i)
                values[i]=static_cast<int16_t>(static_cast<int>((i*7919+v*1237)%65536)-32768);
            clips[v]=Pcm16::make(44100,channels,std::move(values)); CHECK(clips[v]);
            gains[v]=static_cast<float>(v%3)*0.5f;
        }
        for (std::size_t request:{1u,255u,256u,257u,513u}) {
            Mixer m; CHECK(m.configure(44100,channels));
            for (std::size_t v=0;v<9;++v) CHECK(m.start(v,*clips[v],gains[v]));
            std::size_t offset=0;
            const auto bytes=request*channels*2+(channels*2-1);
            std::vector<unsigned char> out(bytes+2);
            for (int call=0;call<4;++call) {
                std::fill(out.begin(),out.end(),0xCD);
                m.render(out.data()+1,bytes);
                CHECK(out.front()==0xCD&&out.back()==0xCD);
                for (std::size_t f=0;f<request;++f) for (unsigned c=0;c<channels;++c) {
                    int64_t sum=0;
                    for (std::size_t v=0;v<9;++v) if (offset+f<clips[v]->layout().frames) {
                        const auto value=clips[v]->at(offset+f,c);
                        sum+=gains[v]==0 ? 0 : gains[v]==0.5f ? value/2 : value;
                    }
                    int16_t got=0; std::memcpy(&got,out.data()+1+(f*channels+c)*2,2);
                    CHECK(got==finish(sum));
                }
                for (std::size_t i=request*channels*2;i<bytes;++i) CHECK(out[i+1]==0);
                offset+=request;
                for (std::size_t v=0;v<9;++v)
                    CHECK(m.cursor_frames(v)==std::min(offset,clips[v]->layout().frames));
            }
        }
    }
}
int main() {
    order_and_bounds(); formats_and_gain(); block_matrix();
    std::puts("Mixing: all S16 scalars, permutations, nine-voice bounds, gain policy, cursors and chunk matrix passed.");
}
