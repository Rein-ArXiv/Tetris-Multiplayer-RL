#include "net/message_stream.h"
#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <vector>
#define CHECK(x) do { if (!(x)) { std::cerr << __LINE__ << ": " #x "\n"; std::exit(1); } } while(false)
using namespace study_net;
std::vector<std::uint8_t> wire() {
    std::vector<std::uint8_t> bytes;
    for (std::uint8_t type : {1, 2, 3}) {
        Frame frame{}; frame.type=type; frame.size=type;
        for (std::size_t i=0;i<frame.size;++i) frame.payload[i]=static_cast<std::uint8_t>(type+i);
        EncodedFrame encoded{}; CHECK(encode_frame(frame,encoded));
        bytes.insert(bytes.end(),encoded.bytes.begin(),encoded.bytes.begin()+encoded.size);
    }
    return bytes;
}
int main() {
    const auto bytes=wire();
    // All two cuts, including empty messages: segmentation must not change data.
    for(std::size_t a=0;a<=bytes.size();++a) for(std::size_t b=a;b<=bytes.size();++b) {
        MessageStream stream(bytes.size()); std::vector<Frame> frames;
        auto sink=[&](const Frame& f){frames.push_back(f);return true;};
        CHECK(stream.accept(true,bytes.data(),a,sink));
        CHECK(stream.accept(true,bytes.data()+a,b-a,sink));
        CHECK(stream.accept(true,bytes.data()+b,bytes.size()-b,sink));
        CHECK(stream.finish());CHECK(frames.size()==3);
        for(std::size_t i=0;i<frames.size();++i) {
            CHECK(frames[i].type==i+1);CHECK(frames[i].size==i+1);
            for(std::size_t j=0;j<frames[i].size;++j)CHECK(frames[i].payload[j]==i+1+j);
        }
    }
    // A WS message can exceed parser storage and contain many complete frames.
    std::vector<std::uint8_t> many;
    for(int i=0;i<100;++i)many.insert(many.end(),bytes.begin(),bytes.end());
    MessageStream large(many.size()); std::size_t count=0;
    CHECK(large.accept(true,many.data(),many.size(),[&](const Frame&){++count;return true;}));
    CHECK(large.finish());CHECK(count==300);
    // One maximum frame split byte by byte, including a null empty message.
    Frame max{};max.type=17;max.size=kMaxPayloadBytes;max.payload.fill(0xff);
    EncodedFrame encoded{};CHECK(encode_frame(max,encoded));
    MessageStream tiny(1);count=0;
    auto sink=[&](const Frame& f){CHECK(f.type==max.type&&f.size==max.size&&f.payload==max.payload);++count;return true;};
    CHECK(tiny.accept(true,nullptr,0,sink));
    for(std::size_t i=0;i<encoded.size;++i)CHECK(tiny.accept(true,encoded.bytes.data()+i,1,sink));
    CHECK(tiny.finish());CHECK(count==1);
    for(int mode=0;mode<4;++mode) {
        MessageStream bad(bytes.size()-1);count=0;
        auto unused=[&](const Frame&){++count;return true;};
        if(mode==0)CHECK(!bad.accept(false,bytes.data(),1,unused));
        if(mode==1)CHECK(!bad.accept(false,nullptr,0,unused));
        if(mode==2)CHECK(!bad.accept(true,nullptr,1,unused));
        if(mode==3)CHECK(!bad.accept(true,bytes.data(),bytes.size(),unused));
        CHECK(count==0);CHECK(bad.failed());CHECK(!bad.finish());
        CHECK(!bad.accept(true,nullptr,0,unused));
    }
    for(std::size_t n=1;n<encoded.size;++n) {
        MessageStream truncated(encoded.size);
        CHECK(truncated.accept(true,encoded.bytes.data(),n,[](const Frame&){return true;}));
        CHECK(truncated.pending_bytes()==n);CHECK(!truncated.finish());
    }
    // Streaming prefix, then malformed length: exactly the valid prefix commits.
    std::vector<std::uint8_t> invalid=bytes;invalid.push_back(0);invalid.push_back(0);
    MessageStream prefix(invalid.size());count=0;
    CHECK(!prefix.accept(true,invalid.data(),invalid.size(),[&](const Frame&){++count;return true;}));
    CHECK(count==3);CHECK(prefix.failed());
    MessageStream rejected(bytes.size());count=0;
    CHECK(!rejected.accept(true,bytes.data(),bytes.size(),[&](const Frame&){++count;return false;}));
    CHECK(count==1);CHECK(rejected.failed());
    // Sink exception propagates; caller discards this adapter rather than retrying.
    bool caught=false;
    try { MessageStream throwing(bytes.size());throwing.accept(true,bytes.data(),bytes.size(),[](const Frame&)->bool {throw std::runtime_error("sink");}); }
    catch(const std::runtime_error&){caught=true;}
    CHECK(caught);
    std::cout<<"message boundaries, payloads, bounds, prefix failure and EOF passed\n";
}
