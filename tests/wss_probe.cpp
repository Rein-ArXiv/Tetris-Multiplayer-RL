// Exercises the same native WSS transport as Session, without rendering or audio.
#include "../net/wss_client.h"
#include "../net/session.h"
#include "../meta/http_client.h"
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <thread>
int main(int argc,char** argv) {
    if(argc==5 && std::string(argv[1])=="--session") {
        // Same issuer callback, worker and shutdown path as the graphical client.
        meta::client::MetaClient meta(argv[3]);
        if(!meta.valid())return 4;
        std::ifstream file(argv[4]);std::string account;file>>account;
        net::Session session;
        session.SetTicketIssuer([&](const std::string& token){return meta.request_game_ticket(token);});
        if(!session.RoomCreate(argv[2],443,120,2,account))return 5;
        auto end=std::chrono::steady_clock::now()+std::chrono::seconds(8);
        while(std::chrono::steady_clock::now()<end) {
            if(session.hasFailed())return 6;
            if(session.roomState()==net::RoomState::Waiting) {
                session.RoomLeave();session.Close();return 0;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        return 7;
    }
    if(argc!=3)return 2;
    std::ifstream file(argv[2],std::ios::binary);
    std::vector<unsigned char> bytes((std::istreambuf_iterator<char>(file)),{});
    if(bytes.empty())return 3;
    auto socket=net::wss_connect(argv[1]);
    if(!socket.valid())return 4;
    if(!net::tcp_send_all(socket,bytes.data(),bytes.size()))return 5;
    const auto end=std::chrono::steady_clock::now()+std::chrono::seconds(5);
    while(std::chrono::steady_clock::now()<end) {
        std::vector<uint8_t> input;
        if(!net::tcp_recv_some(socket,input))return 6;
        if(!input.empty()) {
            std::cout.write(reinterpret_cast<const char*>(input.data()),input.size());
            net::tcp_close(socket);return 0;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    net::tcp_close(socket);return 7;
}
