#include "../core/replay.h"
#include "../core/input.h"
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <limits>
#include <string>
#define CHECK(x) do { if (!(x)) { std::fprintf(stderr,"CHECK failed line %d: %s\n",__LINE__,#x); std::exit(1); } } while(false)
namespace {
const char* path = "replay_io_test.tmp";
struct Cleanup { ~Cleanup(){std::remove(path);} } cleanup;
void write(const std::string& text) {std::ofstream f(path,std::ios::binary);f<<text;CHECK(bool(f));}
std::string contents(){std::ifstream f(path,std::ios::binary);return {std::istreambuf_iterator<char>(f),{}};}
bool same(const ReplayData& a,const ReplayData& b){
    if(a.seed!=b.seed||a.frames.size()!=b.frames.size())return false;
    for(size_t i=0;i<a.frames.size();++i)
        if(a.frames[i].p1!=b.frames[i].p1||a.frames[i].p2!=b.frames[i].p2)return false;
    return true;
}
void rejected(const std::string& text){
    write(text);ReplayData out{77,{{1,2},{8,16}}};const auto before=out;
    CHECK(!ReplayIO::Load(path,out));CHECK(same(out,before));
}
}
int main(){
    CHECK(INPUT_KNOWN_MASK==31);
    for(unsigned value=0;value<256;++value){
        CHECK(isValidInputMask(value)==(value<32));
        write("seed 1\nticks 1\n0 "+std::to_string(value)+" 0\n");
        ReplayData out{88,{{16,1}}};const auto before=out;
        CHECK(ReplayIO::Load(path,out)==(value<32));
        if(value<32)CHECK(out.frames.size()==1&&out.frames[0].p1==value);
        else CHECK(same(out,before));
    }
    for(uint64_t value:{uint64_t(256),uint64_t(257),uint64_t(65536),UINT64_MAX}){
        CHECK(!isValidInputMask(value));
        rejected("seed 1\nticks 1\n0 "+std::to_string(value)+" 0\n");
    }
    for(const std::string bad:{"", "seed 1", "seed 1 ticks 1", "seed 1 ticks 1 0 1",
        "seed 1 ticks 1 1 0 0", "seed 1 ticks 2 0 1 0 0 2 0", "seed 1 ticks 2 0 1 0",
        "seed 1 ticks 1 0 1 0 junk", "seed 1 ticks 0 0 0 0", "seed -1 ticks 0",
        "seed +1 ticks 0", "seed 1 ticks -1", "seed 18446744073709551616 ticks 0",
        "seed 000000000000000000000 ticks 0", "seed 1 ticks 1000001", "seed 1 ticks 18446744073709551615",
        "seed 1 ticks 1 0 -1 0", "seed 1 ticks 1 0 +1 0", "seed 1 ticks 1 0 0x10 0",
        "seed 1 ticks 1 0 1x 0", "seed 1 ticks 1 0 0 32", "seed 1 ticks 1 0 0 256",
        "seeds 1 ticks 0", "seed 1 tick 0"})rejected(bad);
    ReplayData all;all.seed=UINT64_MAX;
    for(unsigned a=0;a<32;++a)for(unsigned b=0;b<32;++b)
        all.frames.push_back({static_cast<uint8_t>(a),static_cast<uint8_t>(b)});
    CHECK(ReplayIO::Save(path,all));ReplayData loaded;CHECK(ReplayIO::Load(path,loaded));CHECK(same(all,loaded));
    write("seed 0001\r\nticks 0001\r\n0000 0009 0031\r\n\t");
    CHECK(ReplayIO::Load(path,loaded)&&loaded.seed==1&&loaded.frames[0].p1==9&&loaded.frames[0].p2==31);
    CHECK(ReplayIO::Save(path,{}));CHECK(ReplayIO::Load(path,loaded)&&loaded.frames.empty());
    write("keep existing contents");const auto before=contents();
    ReplayData invalid{1,{{32,0}}};CHECK(!ReplayIO::Save(path,invalid));CHECK(contents()==before);
    invalid.frames.assign(static_cast<size_t>(ReplayIO::kMaxTicks)+1,{});
    CHECK(!ReplayIO::Save(path,invalid));CHECK(contents()==before);
    invalid.frames.pop_back();CHECK(ReplayIO::Save(path,invalid));
    CHECK(ReplayIO::Load(path,loaded)&&loaded.frames.size()==ReplayIO::kMaxTicks);
    {std::ofstream f(path,std::ios::binary);f.seekp(static_cast<std::streamoff>(ReplayIO::kMaxFileBytes));f.put('x');CHECK(bool(f));}
    const auto retained=loaded;CHECK(!ReplayIO::Load(path,loaded));CHECK(same(loaded,retained));
#ifdef __linux__
    CHECK(!ReplayIO::Save("/dev/full",ReplayData{1,{{0,0}}}));
#endif
    std::puts("Replay IO: masks, narrowing, strict rows, failure preservation, limits, round-trip and write failure passed");
}
