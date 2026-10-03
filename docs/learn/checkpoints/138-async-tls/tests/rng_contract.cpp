#include "core/rng.h"
#include "simulation/piece_source.h"
#include "simulation/round.h"
#include "loop/frame_runner.h"
#include "src/seed_option.h"
#include <array>
#include <limits>
#include <cstdio>
#include <cstdlib>
#define CHECK(x) do { if (!(x)) { std::fprintf(stderr,"CHECK failed at %d: %s\n",__LINE__,#x); std::exit(1); } } while(false)
using U64 = std::uint64_t;
// Independent representation: move individual bits, then multiply by repeated addition.
static U64 shift_xor(U64 value, unsigned distance, bool left) {
    std::array<bool,64> bits{};
    for (unsigned i=0;i<64;++i) bits[i] = (value & (U64{1}<<i)) != 0;
    U64 result=0;
    for (unsigned i=0;i<64;++i) {
        bool moved = left ? (i>=distance && bits[i-distance]) : (i+distance<64 && bits[i+distance]);
        if (bits[i] != moved) result |= U64{1}<<i;
    }
    return result;
}
static U64 oracle(U64& state) {
    state=shift_xor(shift_xor(shift_xor(state,12,false),25,true),27,false);
    U64 product=0, addend=state, factor=2685821657736338717ull;
    while(factor){if(factor&1)product+=addend;addend+=addend;factor>>=1;}
    return product;
}
static void same_source(const study_next::SeededBagSource& a,const study_next::SeededBagSource& b) {
    CHECK(a.rng_state()==b.rng_state() && a.cursor()==b.cursor());
    CHECK(a.bag().remaining()==b.bag().remaining());
    for(std::size_t i=0;i<8;++i)CHECK(a.bag().at(i)==b.bag().at(i));
}
static void same_round(const study_round::Round& a,const study_round::Round& b) {
    CHECK(a.board().cells()==b.board().cells() && a.kind()==b.kind());
    CHECK(a.end_reason()==b.end_reason() && a.quarter()==b.quarter());
    CHECK(a.score()==b.score() && a.total_lines()==b.total_lines());
    CHECK(a.gravity().elapsed==b.gravity().elapsed && a.gravity().interval==b.gravity().interval);
    CHECK(a.active().has_value()==b.active().has_value());
    if(a.active()) {
        CHECK(a.active()->origin.row==b.active()->origin.row && a.active()->origin.column==b.active()->origin.column);
        for(std::size_t i=0;i<4;++i)CHECK(a.active()->local[i].row==b.active()->local[i].row && a.active()->local[i].column==b.active()->local[i].column);
    }
    for(std::size_t i=0;i<3;++i)CHECK(a.next().peek(i)==b.next().peek(i));
    CHECK(a.source().seeded()&&b.source().seeded());same_source(*a.source().seeded(),*b.source().seeded());
}
int main() {
    XorShift64Star one(1);
    const U64 states[]={33554433ull,1126174793148417ull,3659449627584515ull};
    const U64 outputs[]={5180492295206395165ull,12380297144915551517ull,13389498078930870103ull};
    for(int i=0;i<3;++i){CHECK(one.next()==outputs[i]);CHECK(one.getState()==states[i]);}
    for(U64 n=0;n<4096;++n){
        const U64 seed=n*0x9E3779B97F4A7C15ull;
        XorShift64Star rng(seed); U64 expected=seed?seed:88172645463393265ull;
        CHECK(rng.getState()==expected);
        for(int i=0;i<16;++i){CHECK(rng.next()==oracle(expected));CHECK(rng.getState()==expected&&expected!=0);}
        // Copy preserves the current continuation, not the initial seed.
        auto copy=rng;for(int i=0;i<8;++i)CHECK(copy.next()==rng.next());
    }
    for(U64 seed : {U64{0},U64{1},U64{1}<<63,std::numeric_limits<U64>::max()}){
        for(std::uint32_t bound : {0u,1u,2u,3u,7u,10u,std::numeric_limits<std::uint32_t>::max()}) {
            XorShift64Star a(seed),b(seed);const auto output=b.next();
            CHECK(a.nextUInt(bound)==output%(bound?bound:1u));
            CHECK(a.getState()==b.getState());
        }
    }
    CHECK(XorShift64Star(0).getState()==88172645463393265ull);
    CHECK(study_next::SeededBagSource(0).rng_state()==0xC0FFEE123456789ull);
    for(U64 seed=0;seed<100;++seed){
        study_next::SeededBagSource source(seed);
        XorShift64Star reference(seed?seed:study_next::SeededBagSource::default_seed);
        std::array<study_catalog::Kind,7> remaining{};std::size_t count=0;
        for(int n=0;n<1000;++n){
            if(count==0){for(std::size_t i=0;i<7;++i)remaining[i]=study_catalog::definitions[i].kind;count=7;}
            const auto index=reference.nextUInt(static_cast<std::uint32_t>(count));
            const auto expected=remaining[index];
            for(std::size_t i=index;i+1<count;++i)remaining[i]=remaining[i+1];
            --count;
            CHECK(source.next()==expected && source.rng_state()==reference.getState());
            CHECK(source.bag().remaining()==count && source.cursor()==(7-count)%7);
            auto saved=source;auto copy=source;
            (void)copy.next();same_source(source,saved);
            CHECK(source.bag().next_bound()>0); // Views cannot consume state.
        }
        auto external=study_next::SeededBagSource(seed);
        const auto initial=external;
        auto round=study_round::Round::create(study_grid::Grid{},external);CHECK(round);
        same_source(external,initial);auto expected=external;
        CHECK(round->kind()==expected.next());for(int i=0;i<3;++i)CHECK(round->next().peek(i)==expected.next());
        same_source(*round->source().seeded(),expected);
        auto copy=*round;auto before=*round;
        CHECK(round->tick(9,true,false,true)==study_round::Step::invalid);same_round(*round,before);
        // Every lock consumes once, including the one that causes game-over; stopped consumes zero.
        for(int n=0;n<100&&!round->finished();++n){
            auto next_expected=*round->source().seeded();(void)next_expected.next();
            CHECK(round->tick(0,false,false,true)==copy.tick(0,false,false,true));
            same_round(*round,copy);same_source(*round->source().seeded(),next_expected);
        }
        CHECK(round->finished());before=*round;CHECK(round->tick(0,false,false,true)==study_round::Step::stopped);same_round(*round,before);
        // Blocking the initial spawn still creates current plus three previews.
        study_grid::Grid blocked;for(int c=3;c<=6;++c)CHECK(blocked.set(0,c,study_grid::Cell::filled));
        for(int c=3;c<=6;++c)CHECK(blocked.set(1,c,study_grid::Cell::filled));
        auto finished=study_round::Round::create(blocked,external);CHECK(finished&&finished->finished());
        same_source(*finished->source().seeded(),expected);same_source(external,initial);
        CHECK(!study_round::Round::create(blocked,external,0));same_source(external,initial);
        auto fresh=*study_round::Round::create(study_grid::Grid{},external);
        study_loop::FrameRunner runner(fresh);
        CHECK(runner.advance(0,{false,false,false,false,true})->ticks==0);same_round(runner.round(),fresh);
        CHECK(!runner.advance(std::numeric_limits<double>::quiet_NaN(),{}));same_round(runner.round(),fresh);
        auto report=runner.advance(0.02,{});CHECK(report&&report->ticks==1);
        CHECK(fresh.tick(0,false,false,true)==study_round::Step::locked);same_round(runner.round(),fresh);
    }
    const auto scripted=*study_next::ScriptedSource::cycle(study_catalog::Kind::T);
    study_next::PieceSource variant(scripted);CHECK(!variant.seeded());
    variant=study_next::SeededBagSource(1);CHECK(variant.seeded());
    variant=scripted;CHECK(!variant.seeded()&&variant.cursor()==0&&variant.next()==study_catalog::Kind::T);
    CHECK(study_seed::parse("0")==0&&study_seed::parse("00042")==42);
    CHECK(study_seed::parse("18446744073709551615")==std::numeric_limits<U64>::max());
    for(const auto text:{"","-1","+1"," 1","1 ","0x10","1e3","18446744073709551616","42oops"})CHECK(!study_seed::parse(text));
    std::puts("65536 bit-array engine transitions; bound0/1 and high-bit seeds; 100000 bag draws/copies; 100 Round histories including stopped/blocked/invalid; zero-tick and invalid-time pending preservation; variant and decimal boundaries passed");
}
