#include "net/input_codec.h"
#include "net/socket.h"
#include "net/stream.h"
#include "net/send_socket.h"
#include <charconv>
#include <cstdio>
#include <cstring>

namespace {
int fail(const char* reason) { std::fprintf(stderr,"ERROR %s\n",reason);return 1; }
int exchange(std::uint16_t port) {
    int error=0;
    auto socket=study_net::connect_loopback(port,error);
    if(!socket.valid()) {std::fprintf(stderr,"connect error=%d\n",error);return 1;}
    study_net::InputBatch request{};
    request.first_tick=0x01020304u;request.count=3;
    request.masks[0]=study_input::left;request.masks[1]=0;request.masks[2]=study_input::drop;
    study_net::Frame frame;study_net::EncodedFrame bytes;
    if(!study_net::encode_input_payload(request,frame) || !study_net::encode_frame(frame,bytes)) return fail("encode");
    std::printf("PAYLOAD");
    for(std::size_t i=0;i<frame.size;++i) std::printf(" %02X",unsigned(frame.payload[i]));
    std::puts("");
    // This probe exclusively owns the socket. Mode affects both directions.
    if(!study_net::set_nonblocking(socket,true,error)) return fail("nonblocking mode");
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(2);
    const auto report=study_net::send_bounded(socket,bytes.bytes.data(),bytes.size,deadline);
    if(report.outcome!=study_net::SendOutcome::complete) {
        std::fprintf(stderr,"SEND outcome=%d accepted=%zu error=%d\n",int(report.outcome),report.accepted,report.error);
        return 1; // RAII closes; never resend the whole frame on this connection.
    }
    // Receive still uses the blocking diagnostic contract; its wait is not
    // covered by the send deadline. Restore only after checking the send result.
    if(!study_net::set_nonblocking(socket,false,error)) return fail("blocking mode");
    study_net::FrameParser parser;
    bool replied=false;
    for(;;) {
        std::uint8_t scratch[16];
        const auto result=study_net::receive_some(socket,scratch,sizeof(scratch));
        if(result.status==study_net::StreamStatus::error) return fail("receive");
        if(result.status==study_net::StreamStatus::eof) break;
        if(!parser.append(scratch,result.count)) return fail("append");
        for(;;) {
            study_net::Frame reply;
            const auto status=parser.next(reply);
            if(status==study_net::ParseStatus::need_more) break;
            if(status==study_net::ParseStatus::error) return fail("frame");
            if(replied || reply.type!=2) return fail("reply type/count");
            study_net::InputBatch decoded;
            if(!study_net::decode_input_payload(reply,decoded)) return fail("payload grammar");
            if(decoded.first_tick!=request.first_tick || decoded.count!=request.count || decoded.masks!=request.masks) return fail("payload values");
            replied=true;
            // A complete typed response arrived before signalling request EOF.
            if(!study_net::shutdown_send(socket,error)) return fail("shutdown");
        }
    }
    if(!replied || parser.pending_bytes()!=0) return fail("incomplete response");
    std::printf("VERIFIED first_tick=%u count=%zu masks=01,00,10\n",unsigned(request.first_tick),request.count);
    return 0;
}
}
int main(int argc,char** argv) {
    if(argc==2 && std::strcmp(argv[1],"--help")==0) {
        std::puts("serialization_probe PORT (framing_probe listen PORT READ_CAP)");return 0;
    }
    if(argc!=2) return 2;
    unsigned port=0;const char* end=argv[1]+std::strlen(argv[1]);
    const auto parsed=std::from_chars(argv[1],end,port);
    if(parsed.ec!=std::errc{} || parsed.ptr!=end || port==0 || port>65535) return 2;
    study_net::Runtime runtime;
    if(!runtime.ready()) return fail("runtime");
    return exchange(static_cast<std::uint16_t>(port));
}
