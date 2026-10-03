#include "net/framing.h"
#include <cstdio>
#include <cstdlib>
#include <limits>
#define CHECK(x) do { if (!(x)) { std::fprintf(stderr,"line %d: %s\n",__LINE__,#x); std::exit(1); } } while (0)
int main() {
    using namespace study_net;
    // Two frames: type 1 / binary 00 FF, then type 2 / empty. Seven gaps.
    const std::uint8_t wire[]={3,0,1,0,255,1,0,2};
    for(unsigned cuts=0;cuts<128;++cuts) {
        FrameParser parser;std::size_t begin=0,frames=0;
        for(std::size_t end=1;end<=sizeof(wire);++end) {
            if(end!=sizeof(wire) && !(cuts&(1u<<(end-1)))) continue;
            CHECK(parser.append(wire+begin,end-begin));begin=end;
            for(;;) {
                Frame frame{};frame.type=99;frame.size=31;frame.payload.fill(17);
                const auto before=parser.pending_bytes();const auto result=parser.next(frame);
                if(result==ParseStatus::need_more) {
                    CHECK(parser.pending_bytes()==before && frame.type==99 && frame.size==31 && frame.payload[0]==17);break;
                }
                CHECK(result==ParseStatus::frame && frames<2);
                CHECK(frame.type==frames+1 && frame.size==(frames==0?2u:0u));
                if(frames==0) CHECK(frame.payload[0]==0 && frame.payload[1]==255);
                for(std::size_t i=frame.size;i<frame.payload.size();++i) CHECK(frame.payload[i]==0);
                ++frames;
            }
        }
        CHECK(frames==2 && parser.pending_bytes()==0 && !parser.failed());
    }
    Frame max{};max.type=255;max.size=kMaxPayloadBytes;max.payload.fill(42);
    EncodedFrame encoded{};CHECK(encode_frame(max,encoded));CHECK(encoded.size==35 && encoded.bytes[0]==33 && encoded.bytes[1]==0);
    FrameParser parser;Frame decoded{};
    for(std::size_t i=0;i<encoded.size;++i) {
        CHECK(parser.append(encoded.bytes.data()+i,1));
        CHECK(parser.next(decoded)==(i+1==encoded.size?ParseStatus::frame:ParseStatus::need_more));
    }
    CHECK(decoded.type==255 && decoded.size==32 && decoded.payload==max.payload);
    max.size=(std::numeric_limits<std::size_t>::max)();auto saved=encoded;
    CHECK(!encode_frame(max,encoded) && encoded.bytes==saved.bytes && encoded.size==saved.size);
    for(unsigned len: {0u,34u,65535u}) {
        FrameParser bad;const std::uint8_t header[]={static_cast<std::uint8_t>(len),static_cast<std::uint8_t>(len>>8)};
        CHECK(bad.append(header,1));CHECK(bad.next(decoded)==ParseStatus::need_more);
        CHECK(bad.append(header+1,1));decoded.type=99;
        CHECK(bad.next(decoded)==ParseStatus::error && decoded.type==99 && bad.failed() && bad.pending_bytes()==0);
        CHECK(!bad.append(nullptr,0) && bad.next(decoded)==ParseStatus::error);
    }
    FrameParser null;CHECK(null.append(nullptr,0));CHECK(!null.append(nullptr,1) && null.failed());
    FrameParser overflow;CHECK(overflow.append(wire,1));CHECK(!overflow.append(wire,64) && overflow.failed() && overflow.pending_bytes()==0);
    ByteBuffer<8> buffer;CHECK(buffer.append(wire,8));CHECK(!buffer.consume_front(9) && buffer.size()==8);
    CHECK(buffer.consume_front(0) && buffer.size()==8);CHECK(buffer.consume_front(5) && buffer.size()==3);
    CHECK(buffer.data()[0]==1 && buffer.data()[1]==0 && buffer.data()[2]==2);
    CHECK(buffer.consume_front(3) && buffer.size()==0);CHECK(buffer.append(wire,8));
    std::puts("128 partitions, incomplete output preservation, maximum frame, fatal lengths and prefix consumption passed");
}
