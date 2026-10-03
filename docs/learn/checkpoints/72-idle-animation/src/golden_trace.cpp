#include "simulation/state_hash.h"
#include "simulation/input_mask.h"
#include "src/seed_option.h"
#include <cinttypes>
#include <cstdio>
#include <string_view>
namespace {
struct Request { unsigned mask; int garbage; };
// A mask is applied on EVERY tick, unlike the root dump's one-submit/many-ticks script.
constexpr Request mixed[] = {
    {0,0},{0,0},{1,0},{8,0},{4,0},{4,0},{4,0},{4,0},{4,0},
    {2,0},{16,0},{0,3},{8,0},{16,0},{3,0},{9,0},{16,0},
    {0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},
    {0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},
    {0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},
    {0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{16,0}
};
constexpr Request overflow[] = {{16,20},{0,0},{0,0}};
const char* name(study_round::Step value) {
    switch(value) {
    case study_round::Step::waiting:return "waiting";
    case study_round::Step::changed:return "changed";
    case study_round::Step::locked:return "locked";
    case study_round::Step::game_over:return "game_over";
    case study_round::Step::stopped:return "stopped";
    case study_round::Step::invalid:return "invalid";
    }
    return "invalid";
}
bool record(const study_round::Round& round,unsigned tick,Request request,const char* step) {
    const auto bytes=study_hash::state_bytes(round);const auto hash=bytes.digest();
    if(!hash)return false;
    std::printf("%s    {\"tick\":%u,\"mask\":%u,\"garbage\":%d,\"step\":\"%s\",\"hash\":\"%016" PRIx64 "\",\"bytes\":\"",
        tick?",\n":"",tick,request.mask,request.garbage,step,*hash);
    for(std::size_t i=0;i<bytes.size();++i)std::printf("%02x",unsigned(bytes.data()[i]));
    std::printf("\"}");return true;
}
}
int main(int argc,char** argv) {
    const std::string_view scenario=argc>1?argv[1]:"mixed-v1";
    const auto seed=study_seed::parse(argc>2?argv[2]:"1");
    if(argc>3||!seed||(scenario!="mixed-v1"&&scenario!="overflow-v1")) {
        std::fprintf(stderr,"usage: golden_trace [mixed-v1|overflow-v1] [decimal-seed]\n");return 2;
    }
    auto round=study_round::Round::create_seeded(study_grid::Grid{},*seed);if(!round)return 1;
    std::printf("{\n  \"format\":\"LRND-trace/1\",\n  \"rules\":\"checkpoint-52/1\",\n  \"hash_format\":\"LRND/1\",\n  \"scenario\":\"%s\",\n  \"seed\":\"%" PRIu64 "\",\n  \"records\":[\n",scenario=="mixed-v1"?"mixed-v1":"overflow-v1",*seed);
    if(!record(*round,0,{0,0},"initial"))return 1;
    const auto* requests=scenario=="mixed-v1"?mixed:overflow;
    const std::size_t count=scenario=="mixed-v1"?std::size(mixed):std::size(overflow);
    for(std::size_t i=0;i<count;++i) {
        const auto request=requests[i];const auto intent=study_input::decode(request.mask);
        if(!intent||(request.garbage>0&&!round->add_garbage(request.garbage)))return 1;
        const auto result=round->tick(intent->horizontal,intent->clockwise,intent->soft_drop,intent->hard_drop);
        if(result==study_round::Step::invalid||!record(*round,static_cast<unsigned>(i+1),request,name(result)))return 1;
    }
    std::puts("\n  ]\n}");
    return std::fflush(stdout)==0&&!std::ferror(stdout)?0:1;
}
