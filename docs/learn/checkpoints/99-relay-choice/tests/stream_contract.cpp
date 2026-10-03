#include "net/stream.h"
#include "net/byte_buffer.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#define CHECK(x) do { if (!(x)) { std::fprintf(stderr,"failed line %d: %s\n",__LINE__,#x); std::exit(1); } } while (0)
int main() {
    using namespace study_net;
    const std::uint8_t message[]={0,1,2,128,255,42};
    // Every possible partition of six bytes: five gaps, each cut or uncut.
    for (unsigned mask=0;mask<32;++mask) {
        ByteBuffer<6> buffer;
        std::size_t begin=0;
        for (std::size_t end=1;end<=6;++end) {
            if (end==6 || (mask & (1u<<(end-1)))) {
                CHECK(buffer.append(message+begin,end-begin));
                begin=end;
            }
        }
        CHECK(buffer.size()==6 && buffer.remaining()==0);
        CHECK(std::memcmp(buffer.data(),message,6)==0);
        CHECK(!buffer.append(message,1));
        CHECK(buffer.size()==6 && std::memcmp(buffer.data(),message,6)==0);
    }
    ByteBuffer<6> buffer;
    CHECK(buffer.append(nullptr,0));
    CHECK(!buffer.append(nullptr,1));
    CHECK(buffer.append(message,2));
    CHECK(!buffer.append(message,(std::numeric_limits<std::size_t>::max)()));
    CHECK(buffer.size()==2 && buffer.data()[1]==1);
    ByteBuffer<0> empty;
    CHECK(empty.append(nullptr,0) && !empty.append(message,1));
    constexpr auto maximum=static_cast<std::size_t>((std::numeric_limits<int>::max)());
    static_assert(io_chunk_size(maximum+1)==static_cast<int>(maximum));
    static_assert(io_chunk_size((std::numeric_limits<std::size_t>::max)())==static_cast<int>(maximum));
    Runtime runtime;CHECK(runtime.ready());
    int error=0;std::uint16_t port=0;
    auto listener=listen_loopback(0,port,error);CHECK(listener.valid());
    auto client=connect_loopback(port,error);CHECK(client.valid());
    auto server=accept_one(listener,error);CHECK(server.valid());listener.reset();
    std::uint8_t scratch[16]{};
    CHECK(receive_some(client,scratch,0).status==StreamStatus::error);
    CHECK(send_some(client,message,0).status==StreamStatus::error);
    CHECK(receive_some(client,nullptr,1).status==StreamStatus::error);
    Socket invalid;CHECK(send_some(invalid,message,6).status==StreamStatus::error);
    std::size_t sent=0;
    while(sent<6) {
        const auto r=send_some(client,message+sent,6-sent);
        CHECK(r.status==StreamStatus::progress && r.count>0 && r.count<=6-sent && r.error==0);
        sent+=r.count;
    }
    CHECK(shutdown_send(client,error));
    ByteBuffer<6> request;
    for (;;) {
        const auto r=receive_some(server,scratch,2);
        if(r.status==StreamStatus::eof) { CHECK(r.count==0 && r.error==0);break; }
        CHECK(r.status==StreamStatus::progress && r.count>0 && r.count<=2);
        CHECK(request.append(scratch,r.count));
    }
    CHECK(request.size()==6 && std::memcmp(request.data(),message,6)==0);
    // Receiving EOF leaves the reverse direction available for a response.
    auto r=send_some(server,message,1);CHECK(r.status==StreamStatus::progress && r.count==1);
    CHECK(shutdown_send(server,error));
    r=receive_some(client,scratch,16);CHECK(r.status==StreamStatus::progress && r.count==1 && scratch[0]==0);
    r=receive_some(client,scratch,16);CHECK(r.status==StreamStatus::eof && r.count==0);
    std::puts("All32 byte partitions, capacity preservation, binary bytes and half-close response passed.");
}
