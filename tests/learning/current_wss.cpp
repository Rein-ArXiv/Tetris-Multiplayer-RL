// Actual native adapter with deterministic, credential-free loopback echo data.
#include "net/wss_client.h"
#include <chrono>
#include <thread>
#include <vector>
int main(int argc,char** argv) {
    if(argc!=2)return 2;
    if(std::string(argv[1])=="--url-contract") {
        net::WssEndpoint out{"old-host","old-port","old-authority","old-target"};
        const std::string invalid=std::string("wss://localhost")+'\0'+".invalid/play";
        if(net::parse_wss_endpoint(invalid,out))return 8;
        if(out.host!="old-host"||out.port!="old-port"||out.authority!="old-authority"||out.target!="old-target")return 9;
        if(!net::parse_wss_endpoint("wss://localhost:443/play",out))return 10;
        if(out.host!="localhost"||out.port!="443"||out.target!="/play")return 11;
        return 0;
    }
    if(!net::net_init())return 3;
    auto socket=net::wss_connect(argv[1]);if(!socket.valid())return 4;
    const std::vector<unsigned char> expected{1,2,3,4,5};
    if(!net::tcp_send_all(socket,expected.data(),expected.size()))return 5;
    std::vector<unsigned char> received;
    const auto until=std::chrono::steady_clock::now()+std::chrono::seconds(3);
    while(std::chrono::steady_clock::now()<until&&received.size()<expected.size()) {
        if(!net::tcp_recv_some(socket,received))return 6;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    net::tcp_close(socket);return received==expected?0:7;
}
