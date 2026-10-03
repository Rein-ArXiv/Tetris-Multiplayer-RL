#include "net/input_message.h"
#include <cstdio>
#include <cstdlib>
#define CHECK(x) do { if (!(x)) { std::fprintf(stderr,"line %d: %s\n",__LINE__,#x); std::exit(1); } } while (0)
std::vector<uint8_t> packet(uint32_t first,uint16_t count,std::vector<uint8_t> masks) {
    std::vector<uint8_t> bytes;net::le_write_u32(bytes,first);net::le_write_u16(bytes,count);
    bytes.insert(bytes.end(),masks.begin(),masks.end());return bytes;
}
int main() {
    using namespace net;
    const auto max=(std::numeric_limits<uint32_t>::max)();
    for (uint32_t first: {0u,120u,max}) {
        auto p=packet(first,1,{31});InputBatchView view;
        CHECK(decode_input_payload(p,view) && view.first_tick==first && view.count==1 && view.masks==p.data()+6);
    }
    auto p=packet(max-1,2,{0,3});InputBatchView view;
    CHECK(decode_input_payload(p,view));
    const auto old=view;
    for (auto bad: {packet(max-1,3,{0,1,2}),packet(0,0,{}),packet(0,2,{0}),packet(0,1,{0,0}),packet(0,2,{1,32})}) {
        CHECK(!decode_input_payload(bad,view));CHECK(view.first_tick==old.first_tick && view.count==old.count && view.masks==old.masks);
    }
    for(size_t n=0;n<6;++n) {std::vector<uint8_t> shortPayload(n);CHECK(!decode_input_payload(shortPayload,view));}
    auto biggest=packet(0,4090,std::vector<uint8_t>(4090,0));CHECK(decode_input_payload(biggest,view));
    biggest.push_back(0);CHECK(!decode_input_payload(biggest,view));
    for(unsigned bits=0;bits<256;++bits) {
        auto bytes=packet(0,1,{static_cast<uint8_t>(bits)});
        CHECK(decode_input_payload(bytes,view)==(bits<=31));
    }
    const uint32_t raw[]={0,1,0x7fffffffu,0x80000000u,0xffffffffu};
    const int32_t expected[]={0,1,2147483647,(-2147483647-1),-1};
    for(size_t i=0;i<5;++i) {std::vector<uint8_t> bytes;le_write_u32(bytes,raw[i]);CHECK(le_read_i32(bytes.data())==expected[i]);}
    std::puts("Input wire: exact extent, complete-mask validation, tick overflow, output preservation and signed endpoints passed");
}
