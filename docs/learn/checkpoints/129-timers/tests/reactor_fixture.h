#ifndef STUDY_REACTOR_FIXTURE_H
#define STUDY_REACTOR_FIXTURE_H
#include "net/callback_loop.h"
#include "net/receive_socket.h"
#include <chrono>
#include <stdexcept>
namespace study_fixture {
using namespace study_net;
inline void require(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
struct Pair { Socket sender, receiver; };
inline Pair pair() {
    int error=0;std::uint16_t port=0;
    auto listener=listen_loopback(0,port,error);require(listener.valid(),"listen");
    auto sender=connect_loopback(port,error);require(sender.valid(),"connect");
    auto receiver=accept_one(listener,error);require(receiver.valid(),"accept");
    require(set_nonblocking(sender,true,error)&&set_nonblocking(receiver,true,error),"nonblocking");
    return {std::move(sender),std::move(receiver)};
}
inline void send(Socket& socket, std::uint8_t byte) {
    auto result=send_bounded(socket,&byte,1,std::chrono::steady_clock::now()+std::chrono::seconds(2));
    require(result.outcome==SendOutcome::complete,"send");
}
inline ReadyBatch observe_until(StudyReactor& reactor, std::size_t count) {
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(2);
    while(std::chrono::steady_clock::now()<deadline){
        auto batch=reactor.poll(20);require(batch.state!=PollState::error,"poll");
        if(batch.events.size()==count)return batch;
    }
    throw std::runtime_error("observation deadline");
}
} // namespace study_fixture
#endif
