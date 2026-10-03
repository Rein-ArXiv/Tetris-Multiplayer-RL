// Real loopback backpressure with the teaching adapter. Linux/POSIX evidence.
#include "net/send_socket.h"
#include "net/stream.h"
#include <sys/socket.h>
#include <fcntl.h>
#include <vector>
#include <cstdio>
#include <cstdlib>
#define CHECK(x) do { if (!(x)) { std::fprintf(stderr,"line %d: %s\n",__LINE__,#x); std::exit(1); } } while (0)
int main() {
    using namespace study_net;using Clock=std::chrono::steady_clock;
    Runtime runtime;CHECK(runtime.ready());int error=0;uint16_t port=0;
    auto listener=listen_loopback(0,port,error);CHECK(listener.valid());
    auto client=connect_loopback(port,error);CHECK(client.valid());
    auto peer=accept_one(listener,error);CHECK(peer.valid());listener.reset();
    const int small=4096;CHECK(::setsockopt(client.native(),SOL_SOCKET,SO_SNDBUF,&small,sizeof(small))==0);
    const int original=::fcntl(client.native(),F_GETFL,0);CHECK(original>=0);
    CHECK(set_nonblocking(client,true,error));CHECK((::fcntl(client.native(),F_GETFL,0)&O_NONBLOCK)!=0);
    std::vector<uint8_t> bytes(16*1024*1024);for(size_t i=0;i<bytes.size();++i)bytes[i]=uint8_t(i%251);
    const auto report=send_bounded(client,bytes.data(),bytes.size(),Clock::now()+std::chrono::milliseconds(50));
    CHECK(report.outcome==SendOutcome::timed_out && report.accepted>0 && report.accepted<bytes.size());
    // The recipient had not read at all. Only the accepted prefix may now arrive.
    CHECK(shutdown_send(client,error));size_t received=0;uint8_t scratch[16384];
    for(;;) {
        const auto r=receive_some(peer,scratch,sizeof(scratch));
        if(r.status==StreamStatus::eof) break;
        CHECK(r.status==StreamStatus::progress && received+r.count<=report.accepted);
        for(size_t i=0;i<r.count;++i)CHECK(scratch[i]==bytes[received+i]);
        received+=r.count;
    }
    CHECK(received==report.accepted);
    CHECK(set_nonblocking(client,false,error));CHECK(::fcntl(client.native(),F_GETFL,0)==original);
    // Cancellation is observed before a syscall, even on an already shut socket.
    CHECK(set_nonblocking(client,true,error));
    std::atomic_bool cancel{true};auto cancelled=send_bounded(client,bytes.data(),1,Clock::now()+std::chrono::seconds(1),&cancel);
    CHECK(cancelled.outcome==SendOutcome::cancelled && cancelled.accepted==0);
    Socket invalid;CHECK(!set_nonblocking(invalid,true,error));CHECK(try_send(invalid,bytes.data(),1).state==SendState::error);
    CHECK(try_send(client,nullptr,1).state==SendState::error);
    std::printf("Real loopback: backpressure timeout, accepted prefix=%zu verified exactly, mode flags restored\n",received);
}
