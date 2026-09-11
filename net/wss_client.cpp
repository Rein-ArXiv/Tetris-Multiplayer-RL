#include "wss_client.h"
#include <charconv>
#include <cstdio>

namespace net {
bool parse_wss_endpoint(const std::string& url, WssEndpoint& out) {
    if (url.rfind("wss://", 0) != 0 || url.size() > 2048 ||
        url.find_first_of(" \r\n\t?#@\\") != std::string::npos) return false;
    const auto slash = url.find('/', 6);
    WssEndpoint ep;
    ep.authority = url.substr(6, slash == std::string::npos ? slash : slash - 6);
    ep.target = slash == std::string::npos ? "/play" : url.substr(slash);
    if (ep.target != "/play" || ep.authority.empty()) return false;
    ep.port = "443";
    if (ep.authority.front() == '[') {
        auto end = ep.authority.find(']');
        if (end == std::string::npos || end == 1) return false;
        ep.host = ep.authority.substr(1,end-1);
        if (end+1 < ep.authority.size()) {
            if (ep.authority[end+1] != ':') return false;
            ep.port = ep.authority.substr(end+2);
        }
    } else {
        auto colon = ep.authority.find(':');
        ep.host = ep.authority.substr(0,colon);
        if (colon != std::string::npos) ep.port = ep.authority.substr(colon+1);
        if (ep.host.empty()) return false;
    }
    unsigned port=0;
    auto parsed=std::from_chars(ep.port.data(),ep.port.data()+ep.port.size(),port);
    if (parsed.ec!=std::errc{} || parsed.ptr!=ep.port.data()+ep.port.size() || port==0 || port>65535) return false;
    out=std::move(ep);return true;
}
bool secure_game_endpoint(const std::string& host) {
    WssEndpoint ep;
    return parse_wss_endpoint(host,ep) || host=="127.0.0.1" || host=="::1" || host=="localhost";
}
TcpSocket game_connect(const std::string& host,uint16_t port) {
    if (host.rfind("wss://",0)==0) return wss_connect(host);
    return tcp_connect(host,port);
}
}

#ifdef TETRIS_HAS_WSS
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/beast.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/websocket/ssl.hpp>
#include <openssl/ssl.h>
#include "system_trust.h"
#include <atomic>
#include <cstdlib>
#include <deque>
#include <future>
#include <mutex>
#include <thread>

namespace net {
namespace {
namespace asio=boost::asio;
namespace beast=boost::beast;
namespace websocket=beast::websocket;
using tcp=asio::ip::tcp;
using Error=boost::system::error_code;

class WssClient final : public StreamTransport {
    asio::io_context io_;
    asio::ssl::context tls_{asio::ssl::context::tls_client};
    websocket::stream<beast::ssl_stream<beast::tcp_stream>, false> ws_{io_,tls_};
    tcp::resolver resolver_{io_};
    asio::steady_timer deadline_{io_};
    beast::flat_buffer incoming_;
    WssEndpoint endpoint_;
    std::thread worker_;
    std::promise<bool> connected_;
    bool reported_=false; // io_ thread only
    std::atomic<bool> live_{false};
    std::atomic<size_t> queued_{0};
    std::deque<std::vector<uint8_t>> outgoing_; // io_ thread only
    std::mutex receiveMutex_;
    std::vector<uint8_t> received_;
    static constexpr size_t kQueueLimit=128*1024;

    void report(bool ok) { if(!reported_) {reported_=true;connected_.set_value(ok);} }
    void stop() {
        live_=false;report(false);
        resolver_.cancel();deadline_.cancel();
        Error ignored;
        beast::get_lowest_layer(ws_).socket().cancel(ignored);
        beast::get_lowest_layer(ws_).socket().close(ignored);
        // Beast may still own an idle timer after the socket is cancelled. This
        // private context belongs to this connection only: stop it so destruction
        // never waits for that timer. The caller joins before releasing the stream.
        io_.stop();
    }
    bool failed(Error error) { if(error){stop();return true;}return false; }
    void read() {
        ws_.async_read(incoming_,[this](Error error,size_t size) {
            if(failed(error))return;
            if(!ws_.got_binary()){stop();return;}
            {
                std::lock_guard<std::mutex> lock(receiveMutex_);
                if(received_.size()+size>kQueueLimit){stop();return;}
                auto old=received_.size();received_.resize(old+size);
                asio::buffer_copy(asio::buffer(received_.data()+old,size),incoming_.data());
            }
            incoming_.consume(incoming_.size());
            read();
        });
    }
    void write() {
        ws_.async_write(asio::buffer(outgoing_.front()),[this](Error error,size_t) {
            if(failed(error))return;
            queued_.fetch_sub(outgoing_.front().size());outgoing_.pop_front();
            if(!outgoing_.empty())write();
        });
    }
public:
    explicit WssClient(WssEndpoint endpoint):endpoint_(std::move(endpoint)) {
        tls_.set_default_verify_paths();
        load_native_trust(tls_.native_handle());
        if(const char* ca=std::getenv("TETRIS_CA_FILE")) tls_.load_verify_file(ca);
        tls_.set_verify_mode(asio::ssl::verify_peer);
        ws_.next_layer().set_verify_mode(asio::ssl::verify_peer);
        ws_.next_layer().set_verify_callback(asio::ssl::host_name_verification(endpoint_.host));
        if(!SSL_set_tlsext_host_name(ws_.next_layer().native_handle(),endpoint_.host.c_str()))
            throw std::runtime_error("TLS server name setup failed");
        SSL_set_min_proto_version(ws_.next_layer().native_handle(),TLS1_2_VERSION);
    }
    bool connect() {
        auto result=connected_.get_future();
        deadline_.expires_after(std::chrono::seconds(5));
        deadline_.async_wait([this](Error error){if(!error)stop();});
        resolver_.async_resolve(endpoint_.host,endpoint_.port,[this](Error error,tcp::resolver::results_type addresses) {
            if(failed(error))return;
            beast::get_lowest_layer(ws_).async_connect(addresses,[this](Error error,const tcp::endpoint&) {
                if(failed(error))return;
                // Match raw TCP's latency policy for small 60 Hz INPUT frames.
                beast::get_lowest_layer(ws_).socket().set_option(tcp::no_delay(true),error);
                if(failed(error))return;
                ws_.next_layer().async_handshake(asio::ssl::stream_base::client,[this](Error error) {
                    if(failed(error))return;
                    ws_.set_option(websocket::stream_base::timeout{std::chrono::seconds(5),std::chrono::seconds(30),true});
                    ws_.read_message_max(16*1024);ws_.binary(true);
                    ws_.async_handshake(endpoint_.authority,endpoint_.target,[this](Error error) {
                        if(failed(error))return;
                        deadline_.cancel();live_=true;report(true);read();
                    });
                });
            });
        });
        worker_=std::thread([this]{try{io_.run();}catch(...){stop();}});
        return result.get();
    }
    ~WssClient() override {close();if(worker_.joinable())worker_.join();}
    bool alive() const override {return live_.load();}
    bool send(const void* data,size_t size) override {
        if(!alive() || size>16*1024)return false;
        auto before=queued_.fetch_add(size);
        if(before+size>kQueueLimit){queued_.fetch_sub(size);close();return false;}
        const auto* p=static_cast<const uint8_t*>(data);
        std::vector<uint8_t> bytes(p,p+size);
        asio::post(io_,[this,bytes=std::move(bytes)]() mutable {
            if(!live_)return;
            bool idle=outgoing_.empty();outgoing_.push_back(std::move(bytes));
            if(idle)write();
        });
        return true;
    }
    bool receive(std::vector<uint8_t>& out) override {
        std::lock_guard<std::mutex> lock(receiveMutex_);
        if(received_.empty())return alive();
        out.insert(out.end(),received_.begin(),received_.end());received_.clear();return true;
    }
    void close() override {
        live_=false;
        asio::post(io_,[this]{stop();});
    }
};
}
TcpSocket wss_connect(const std::string& url) {
    WssEndpoint endpoint;
    if(!parse_wss_endpoint(url,endpoint))return {};
    try {
        auto stream=std::make_shared<WssClient>(std::move(endpoint));
        if(!stream->connect())return {};
        TcpSocket socket;socket.transport=std::move(stream);return socket;
    } catch(const std::exception&) {
        std::fprintf(stderr,"[wss] secure connection failed (certificate, URL or network)\n");
        return {};
    }
}
}
#else
namespace net {
TcpSocket wss_connect(const std::string&) {
    std::fprintf(stderr,"[wss] rebuild with TETRIS_BUILD_WSS=ON\n");return {};
}
}
#endif
