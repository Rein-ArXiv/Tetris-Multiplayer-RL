// Blocking loopback diagnostic. Run under an external timeout; do not call from UI.
#include "net/message_stream.h"
#include "net/tls_identity.h"
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/beast.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/websocket/ssl.hpp>
#include <charconv>
#include <iostream>
#include <string>
#include <vector>
namespace asio=boost::asio;
namespace beast=boost::beast;
namespace websocket=beast::websocket;
using tcp=asio::ip::tcp;
using namespace study_net;
constexpr std::size_t kMessageLimit=kReceiveCapacity*8;
std::vector<Frame> expected_frames() {
    std::vector<Frame> result;
    for(std::uint8_t type:{11,12,13}) {
        Frame f{};f.type=type;f.size=kMaxPayloadBytes;
        for(std::size_t i=0;i<f.size;++i)f.payload[i]=static_cast<std::uint8_t>(type+i);
        result.push_back(f);
    }
    return result;
}
int main(int argc,char** argv) {
    if(argc!=5&&argc!=6) {
        std::cerr<<"wss_roundtrip host port ca.pem split|bundle|fragment [origin]\n";return 2;
    }
    const std::string host=argv[1],port=argv[2],mode=argv[4];
    unsigned port_value=0;auto parsed=std::from_chars(port.data(),port.data()+port.size(),port_value);
    if(host.empty()||parsed.ec!=std::errc{}||parsed.ptr!=port.data()+port.size()||port_value==0||port_value>65535||
       (mode!="split"&&mode!="bundle"&&mode!="fragment"))return 2;
    try {
        asio::io_context io;asio::ssl::context tls(asio::ssl::context::tls_client);
        tls.load_verify_file(argv[3]);tls.set_verify_mode(asio::ssl::verify_peer);
        websocket::stream<beast::ssl_stream<beast::tcp_stream>,false> ws(io,tls);
        ws.next_layer().set_verify_callback([&host](bool preverified,asio::ssl::verify_context& ctx) {
            if(!preverified)return false;
            auto* store=ctx.native_handle();
            return X509_STORE_CTX_get_error_depth(store)>0 ||
                tls_identity::matches_host(X509_STORE_CTX_get_current_cert(store),host);
        });
        boost::system::error_code address_error;asio::ip::make_address(host,address_error);
        if(address_error && !SSL_set_tlsext_host_name(ws.next_layer().native_handle(),host.c_str()))return 3;
        if(!SSL_set_min_proto_version(ws.next_layer().native_handle(),TLS1_2_VERSION))return 3;
        tcp::resolver resolver(io);
        beast::get_lowest_layer(ws).connect(resolver.resolve(host,port));
        ws.next_layer().handshake(asio::ssl::stream_base::client);
        ws.read_message_max(kMessageLimit);ws.binary(true);
        if(argc==6)ws.set_option(websocket::stream_base::decorator([origin=std::string(argv[5])](websocket::request_type& req) {
            req.set(beast::http::field::origin,origin);
        }));
        const auto authority=(host.find(':')!=std::string::npos?"["+host+"]":host)+":"+port;
        ws.handshake(authority,"/play");
        const auto expected=expected_frames();std::vector<std::uint8_t> bytes;
        for(const auto& f:expected) {
            EncodedFrame e{};if(!encode_frame(f,e))return 3;
            bytes.insert(bytes.end(),e.bytes.begin(),e.bytes.begin()+e.size);
        }
        if(mode=="split") {
            // Split an application length header across complete WS messages.
            ws.write(asio::buffer(bytes.data(),1));
            ws.write(asio::buffer(bytes.data()+1,bytes.size()-1));
        } else if(mode=="fragment") {
            // One WS message fragmented around a control frame.
            ws.write_some(false,asio::buffer(bytes.data(),1));
            ws.ping(websocket::ping_data("study"));
            ws.write_some(true,asio::buffer(bytes.data()+1,bytes.size()-1));
        } else ws.write(asio::buffer(bytes));
        MessageStream stream(kMessageLimit);std::size_t received=0;
        auto sink=[&](const Frame& f) {
            if(received>=expected.size())return false;
            const auto& e=expected[received];
            if(f.type!=e.type||f.size!=e.size||f.payload!=e.payload)return false;
            ++received;return true;
        };
        while(received<expected.size()) {
            beast::flat_buffer message;ws.read(message);
            std::vector<std::uint8_t> chunk(message.size());
            asio::buffer_copy(asio::buffer(chunk),message.data());
            if(!stream.accept(ws.got_binary(),chunk.data(),chunk.size(),sink))return 5;
        }
        ws.close(websocket::close_code::normal);
        if(!stream.finish())return 5;
        std::cout<<"ordered application frames restored: "<<mode<<"\n";return 0;
    } catch(const std::exception& error) {
        std::cerr<<"WSS diagnostic failed: "<<error.what()<<"\n";return 4;
    }
}
