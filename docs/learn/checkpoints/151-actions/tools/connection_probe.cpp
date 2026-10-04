#include "net/connection.h"
#include "net/input_codec.h"
#include <charconv>
#include <cstdio>
#include <cstring>
int main(int argc,char** argv) {
    if(argc!=2) return 2;
    unsigned port=0;const char* end=argv[1]+std::strlen(argv[1]);
    const auto parsed=std::from_chars(argv[1],end,port);
    if(parsed.ec!=std::errc{} || parsed.ptr!=end || port==0 || port>65535) return 2;
    study_net::Runtime runtime;
    if(!runtime.ready()) return 1;
    // Runtime is declared first and destroyed last; all socket calls finish first.
    study_net::Connection connection;
    if(connection.start(static_cast<uint16_t>(port)).state!=study_net::Connection::StartState::started) return 1;
    study_net::InputBatch request{};request.first_tick=42;request.count=1;request.masks[0]=study_input::left;
    study_net::Frame frame;
    if(!study_net::encode_input_payload(request,frame)) return 1;
    auto report=connection.send_frame(frame,std::chrono::steady_clock::now()+std::chrono::seconds(2));
    if(report.outcome!=study_net::SendOutcome::complete) return 1;
    int error=0;
    if(!connection.finish_sending(error)) return 1;
    study_net::Frame reply;
    if(connection.next_frame(reply).state!=study_net::Connection::ReadState::frame || reply.type!=2) return 1;
    study_net::InputBatch decoded{};
    if(!study_net::decode_input_payload(reply,decoded) || decoded.first_tick!=request.first_tick || decoded.count!=request.count || decoded.masks!=request.masks) return 1;
    // Exact one-response protocol: EOF alone never replaces semantic validation.
    if(connection.next_frame(reply).state!=study_net::Connection::ReadState::eof) return 1;
    std::printf("VERIFIED tick=%u count=%zu send_closed=%d peer_eof=%d\n",unsigned(decoded.first_tick),decoded.count,connection.send_closed(),connection.peer_eof());
    connection.close();
    std::printf("CLOSED active=%d pending=%zu\n",connection.active(),connection.pending_bytes());
}
