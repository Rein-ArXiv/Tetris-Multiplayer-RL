// WSS boundary for native and future browser clients. Boost.Beast owns RFC 6455
// parsing and OpenSSL owns TLS; Tetris's byte protocol is forwarded unchanged.
// One asynchronous read/write in each direction bounds buffering and applies
// backpressure. All handlers run on one io_context thread.
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/beast.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/websocket/ssl.hpp>
#include <openssl/ssl.h>
#include <array>
#include <charconv>
#include <chrono>
#include <csignal>
#include <iostream>
#include <memory>
#include <set>
#include <string>
#include <unordered_map>

namespace asio=boost::asio;
namespace beast=boost::beast;
namespace http=beast::http;
namespace websocket=beast::websocket;
using tcp=asio::ip::tcp;
using Error=boost::system::error_code;
using Clock=std::chrono::steady_clock;

struct Limits {
    size_t total=0;
    std::unordered_map<std::string,size_t> perIp;
    std::set<std::string> origins;
    unsigned short backendPort=7777;
};
std::string admission_key(asio::ip::address address) {
    if(address.is_v4()) return address.to_string();
    auto bytes=address.to_v6().to_bytes();
    if(address.to_v6().is_v4_mapped()) {
        asio::ip::address_v4::bytes_type v4{{bytes[12],bytes[13],bytes[14],bytes[15]}};
        return asio::ip::address_v4(v4).to_string();
    }
    for(size_t i=8;i<16;++i) bytes[i]=0; // one IPv6 /64 shares an admission budget
    return asio::ip::address_v6(bytes).to_string();
}

class Connection : public std::enable_shared_from_this<Connection> {
    websocket::stream<beast::ssl_stream<beast::tcp_stream>, false> ws_;
    tcp::socket backend_;
    asio::steady_timer deadline_;
    beast::flat_buffer handshakeBuffer_, clientBuffer_;
    http::request_parser<http::empty_body> request_;
    std::array<unsigned char,16*1024> backendBuffer_{};
    Limits& limits_;
    std::string key_;
    bool closed_=false;
    Clock::time_point budgetAt_=Clock::now();
    double tokens_=128*1024;

    bool failed(Error error) {if(error){close();return true;}return false;}
    bool charge(size_t size) {
        auto now=Clock::now();
        tokens_=std::min(128.0*1024,tokens_+std::chrono::duration<double>(now-budgetAt_).count()*64*1024);
        budgetAt_=now;
        if(tokens_<size)return false;
        tokens_-=size;return true;
    }
    void from_client() {
        ws_.async_read(clientBuffer_,[self=shared_from_this()](Error error,size_t size) {
            if(self->failed(error))return;
            if(!self->ws_.got_binary() || !self->charge(size)){self->close();return;}
            asio::async_write(self->backend_,self->clientBuffer_.data(),
                [self](Error error,size_t) {
                    if(self->failed(error))return;
                    self->clientBuffer_.consume(self->clientBuffer_.size());
                    self->from_client();
                });
        });
    }
    void from_backend() {
        backend_.async_read_some(asio::buffer(backendBuffer_),[self=shared_from_this()](Error error,size_t size) {
            if(self->failed(error))return;
            self->ws_.async_write(asio::buffer(self->backendBuffer_.data(),size),
                [self](Error error,size_t) {if(!self->failed(error))self->from_backend();});
        });
    }
    void upgrade() {
        http::async_read(ws_.next_layer(),handshakeBuffer_,request_,
            [self=shared_from_this()](Error error,size_t) {
                if(self->failed(error))return;
                const auto& req=self->request_.get();
                const std::string origin(req[http::field::origin]);
                // Native clients send no Origin and authenticate with the same
                // single-use ticket. Browser origins must be explicitly listed.
                if(req.target()!="/play" || !websocket::is_upgrade(req) ||
                   req.count(http::field::origin)>1 ||
                   (req.count(http::field::origin)>0 && !self->limits_.origins.count(origin)) ||
                   self->handshakeBuffer_.size()!=0) {self->close();return;}
                self->ws_.set_option(websocket::stream_base::timeout{
                    std::chrono::seconds(5),std::chrono::seconds(30),true});
                self->ws_.read_message_max(16*1024);
                self->ws_.binary(true);
                self->ws_.async_accept(req,[self](Error error) {
                    if(self->failed(error))return;
                    self->backend_.async_connect({asio::ip::address_v4::loopback(),self->limits_.backendPort},
                        [self](Error error) {
                            if(self->failed(error))return;
                            self->backend_.set_option(tcp::no_delay(true),error);
                            if(self->failed(error))return;
                            self->deadline_.cancel();self->from_client();self->from_backend();
                        });
                });
            });
    }
public:
    Connection(tcp::socket socket,asio::ssl::context& tls,Limits& limits,std::string key)
        :ws_(std::move(socket),tls),backend_(ws_.get_executor()),deadline_(ws_.get_executor()),
         limits_(limits),key_(std::move(key)) {request_.header_limit(8192);request_.body_limit(0);}
    ~Connection() {
        --limits_.total;
        auto it=limits_.perIp.find(key_);
        if(it!=limits_.perIp.end() && --it->second==0)limits_.perIp.erase(it);
    }
    void run() {
        Error error;
        beast::get_lowest_layer(ws_).socket().set_option(tcp::no_delay(true),error);
        if(failed(error))return;
        deadline_.expires_after(std::chrono::seconds(5));
        deadline_.async_wait([self=shared_from_this()](Error error){if(!error)self->close();});
        ws_.next_layer().async_handshake(asio::ssl::stream_base::server,
            [self=shared_from_this()](Error error){if(!self->failed(error))self->upgrade();});
    }
    void close() {
        if(closed_)return;closed_=true;
        deadline_.cancel();Error ignored;
        backend_.cancel(ignored);backend_.close(ignored);
        auto& socket=beast::get_lowest_layer(ws_).socket();
        socket.cancel(ignored);socket.close(ignored);
    }
};

unsigned short port_number(const std::string& text) {
    unsigned value=0;auto r=std::from_chars(text.data(),text.data()+text.size(),value);
    if(r.ec!=std::errc{} || r.ptr!=text.data()+text.size() || value==0 || value>65535)
        throw std::runtime_error("port must be 1..65535");
    return static_cast<unsigned short>(value);
}
int main(int argc,char** argv) {
    try {
        std::string cert,key,bind="0.0.0.0";
        unsigned short port=8443;
        Limits limits;
        for(int i=1;i<argc;++i) {
            std::string arg=argv[i];
            if(arg=="--help") {
                std::cout<<"tetris_wss_gateway --cert chain.pem --key key.pem [--port 8443] [--bind 0.0.0.0]\n"
                         <<"  [--backend-port 7777] [--origin https://game.example.com] (repeatable)\n"
                         <<"Backend is always 127.0.0.1. Start relay with --loopback-only.\n";return 0;
            }
            if(i+1==argc)throw std::runtime_error("missing option value");
            std::string value=argv[++i];
            if(arg=="--cert")cert=value;
            else if(arg=="--key")key=value;
            else if(arg=="--port")port=port_number(value);
            else if(arg=="--backend-port")limits.backendPort=port_number(value);
            else if(arg=="--bind")bind=value;
            else if(arg=="--origin") {
                if(value.rfind("https://",0)!=0 || value.find_first_of("*\r\n\t ,")!=std::string::npos)
                    throw std::runtime_error("origin must be an exact HTTPS origin");
                limits.origins.insert(value);
            } else throw std::runtime_error("unknown option");
        }
        if(cert.empty() || key.empty())throw std::runtime_error("TLS certificate and private key are required");
        asio::io_context io;
        asio::ssl::context tls(asio::ssl::context::tls_server);
        SSL_CTX_set_min_proto_version(tls.native_handle(),TLS1_2_VERSION);
        tls.use_certificate_chain_file(cert);tls.use_private_key_file(key,asio::ssl::context::pem);
        if(SSL_CTX_check_private_key(tls.native_handle())!=1)throw std::runtime_error("certificate/key mismatch");
        tcp::acceptor acceptor(io,{asio::ip::make_address(bind),port});
        std::vector<std::weak_ptr<Connection>> sessions;
        std::function<void()> accept;
        accept=[&] {
            acceptor.async_accept([&](Error error,tcp::socket socket) {
                if(!acceptor.is_open())return;
                if(!error) {
                    auto key=admission_key(socket.remote_endpoint().address());
                    auto found=limits.perIp.find(key);
                    if(limits.total<128 && (found==limits.perIp.end() || found->second<16)) {
                        ++limits.total;++limits.perIp[key];
                        auto session=std::make_shared<Connection>(std::move(socket),tls,limits,key);
                        sessions.erase(std::remove_if(sessions.begin(),sessions.end(),[](const auto& w){return w.expired();}),sessions.end());
                        sessions.push_back(session);session->run();
                    }
                }
                accept();
            });
        };
        asio::signal_set signals(io,SIGINT,SIGTERM);
        signals.async_wait([&](Error,int){
            Error ignored;acceptor.close(ignored);
            for(auto& weak:sessions)if(auto session=weak.lock())session->close();
        });
        accept();std::cout<<"[wss] TLS gateway listening on "<<bind<<":"<<port<<"\n"<<std::flush;
        io.run();
    } catch(const std::exception& error) {std::cerr<<"[wss] "<<error.what()<<"\n";return 1;}
}
