#include "net/input_codec.h"
#include <cstdio>
#include <cstdlib>
#include <algorithm>
#define CHECK(x) do { if (!(x)) { std::fprintf(stderr,"line %d: %s\n",__LINE__,#x); std::exit(1); } } while (0)
int main() {
    using namespace study_net;
    const std::uint8_t golden[]={0xab,0x34,0x12,0x78,0x56,0x34,0x12,0xef,0xcd,0xab,0x89,0x67,0x45,0x23,0x01};
    std::array<std::uint8_t,15> storage{};ByteWriter writer(storage.data(),storage.size());
    CHECK(writer.u8(0xab) && writer.u16(0x1234) && writer.u32(0x12345678) && writer.u64(0x0123456789abcdefULL));
    CHECK(std::equal(storage.begin(),storage.end(),golden));auto saved=storage;
    CHECK(!writer.u8(0) && writer.position()==15 && storage==saved);
    ByteReader reader(golden,sizeof(golden));std::uint8_t a;std::uint16_t b;std::uint32_t c;std::uint64_t d;
    CHECK(reader.u8(a) && reader.u16(b) && reader.u32(c) && reader.u64(d) && reader.at_end());
    CHECK(a==0xab && b==0x1234 && c==0x12345678 && d==0x0123456789abcdefULL);
    for(std::size_t n=0;n<8;++n) {
        ByteReader shortRead(golden,n);d=42;CHECK(!shortRead.u64(d) && d==42 && shortRead.position()==0);
        storage.fill(17);auto before=storage;ByteWriter shortWrite(storage.data(),n);
        CHECK(!shortWrite.u64(42) && shortWrite.position()==0 && storage==before);
    }
    ByteReader empty(nullptr,0),bad(nullptr,1);a=99;
    CHECK(empty.valid() && empty.at_end() && !empty.u8(a) && a==99);
    CHECK(!bad.valid() && !bad.at_end() && !bad.u8(a));
    ByteWriter emptyWrite(nullptr,0),badWrite(nullptr,1);CHECK(emptyWrite.valid() && !emptyWrite.u8(1) && !badWrite.valid() && !badWrite.u8(1));
    // Every u16 value against independent byte positions, then round-trip.
    for(unsigned value=0;value<65536;++value) {
        ByteWriter w(storage.data(),2);CHECK(w.u16(static_cast<std::uint16_t>(value)));
        CHECK(storage[0]==value%256 && storage[1]==value/256);
        ByteReader r(storage.data(),2);CHECK(r.u16(b) && b==value);
    }
    for(std::uint64_t value: {std::uint64_t{0},std::uint64_t{1},std::uint64_t{0x8000000000000000ULL},(std::numeric_limits<std::uint64_t>::max)()}) {
        ByteWriter w(storage.data(),8);CHECK(w.u64(value));
        ByteReader r(storage.data(),8);CHECK(r.u64(d) && d==value);
        for(std::size_t i=0;i<8;++i) CHECK(storage[i]==((value>>(8*i))&255));
    }
    ByteReader fieldReader(golden,3);CHECK(fieldReader.u8(a));c=42;
    CHECK(!fieldReader.u32(c) && c==42 && fieldReader.position()==1);
    storage.fill(17);ByteWriter fieldWriter(storage.data(),3);CHECK(fieldWriter.u8(42));auto afterFirst=storage;
    CHECK(!fieldWriter.u32(0x12345678) && fieldWriter.position()==1 && storage==afterFirst);
    InputBatch input{};input.first_tick=0x01020304u;input.count=3;input.masks[0]=1;input.masks[1]=0;input.masks[2]=16;
    Frame frame;CHECK(encode_input_payload(input,frame));
    const std::uint8_t inputGolden[]={4,3,2,1,3,0,1,0,16};
    CHECK(frame.type==1 && frame.size==9 && std::equal(inputGolden,inputGolden+9,frame.payload.begin()));
    InputBatch output;CHECK(decode_input_payload(frame,output) && output.first_tick==input.first_tick && output.count==input.count && output.masks==input.masks);
    for(std::size_t n=0;n<9;++n) {auto shortFrame=frame;shortFrame.size=n;CHECK(!decode_input_payload(shortFrame,output) && output.masks==input.masks && output.count==3 && output.first_tick==input.first_tick);}
    for(auto corrupt: {4,8,9,32}) {
        auto broken=frame;
        if(corrupt==4) broken.payload[4]=17;
        else if(corrupt==8) {broken.payload[0]=99;broken.payload[8]=32;}
        else broken.size=std::size_t(corrupt)+1;
        CHECK(!decode_input_payload(broken,output) && output.masks==input.masks && output.count==input.count && output.first_tick==input.first_tick);
    }
    input.first_tick=(std::numeric_limits<std::uint32_t>::max)();input.count=2;
    const auto oldFrame=frame;CHECK(!encode_input_payload(input,frame) && frame.payload==oldFrame.payload && frame.size==oldFrame.size && frame.type==oldFrame.type);
    input.count=1;CHECK(encode_input_payload(input,frame));input.count=0;CHECK(!encode_input_payload(input,frame));
    input.first_tick=0;input.count=16;input.masks.fill(31);CHECK(encode_input_payload(input,frame) && frame.size==22 && decode_input_payload(frame,output));
    for(unsigned mask=0;mask<256;++mask) {input.masks[15]=static_cast<std::uint8_t>(mask);CHECK(encode_input_payload(input,frame)==(mask<=31));}
    std::puts("Codec: frozen LE vectors, all u16 values, field/message failure preservation, exact count and semantic limits passed");
}
