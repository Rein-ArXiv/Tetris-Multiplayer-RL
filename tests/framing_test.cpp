#include "net/framing.h"
#include <cstdio>
#include <cstdlib>
#define CHECK(x) do { if (!(x)) { std::fprintf(stderr,"line %d: %s\n",__LINE__,#x); std::exit(1); } } while (0)
int main() {
    using namespace net;
    auto a=build_frame(MsgType::HELLO,{}),b=build_frame(MsgType::INPUT,{0,255});
    CHECK(a==std::vector<uint8_t>({1,0,1,0,0,0,0}));
    auto wire=a;wire.insert(wire.end(),b.begin(),b.end());CHECK(wire.size()==16);
    for(unsigned cuts=0;cuts<(1u<<15);++cuts) {
        std::vector<uint8_t> pending;std::vector<Frame> out;size_t begin=0;
        for(size_t end=1;end<=wire.size();++end) {
            if(end!=wire.size() && !(cuts&(1u<<(end-1)))) continue;
            pending.insert(pending.end(),wire.begin()+begin,wire.begin()+end);begin=end;
            CHECK(parse_frames(pending,out));
        }
        CHECK(pending.empty() && out.size()==2 && out[0].type==MsgType::HELLO && out[0].payload.empty());
        CHECK(out[1].type==MsgType::INPUT && out[1].payload==std::vector<uint8_t>({0,255}));
    }
    std::vector<Frame> out;auto bad=a;bad.push_back(2);bad.push_back(16); // LEN4098
    CHECK(!parse_frames(bad,out) && bad.empty() && out.size()==1);
    std::vector<uint8_t> zero(6,0);out.clear();CHECK(parse_frames(zero,out) && zero.empty() && out.empty());
    bad=b;bad.back()^=1;bad.insert(bad.end(),a.begin(),a.end());
    CHECK(parse_frames(bad,out) && bad.empty() && out.size()==1 && out[0].type==MsgType::HELLO);
    out.clear();auto unknown=build_frame(static_cast<MsgType>(255),{});
    CHECK(parse_frames(unknown,out) && out.size()==1 && static_cast<uint8_t>(out[0].type)==255);
    out.clear();std::vector<uint8_t> payload(kMaxPayloadBytes,42);auto max=build_frame(MsgType::CHAT,payload);
    CHECK(max.size()==4103 && parse_frames(max,out) && out.size()==1 && out[0].payload==payload);
    payload.push_back(42);CHECK(build_frame(MsgType::CHAT,payload).empty());
    std::puts("root framing: 32768 partitions and wire/error policies passed");
}
