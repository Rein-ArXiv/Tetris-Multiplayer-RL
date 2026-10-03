#include "server/ip_admission.h"
#include "server/player_session.h"
#include <array>
#include <atomic>
#include <cstdio>
#include <stdexcept>
#include <thread>
void require(bool ok){if(!ok)throw std::runtime_error("admission contract");}
int main(){
    using Ip=relay::IpAdmission;using Player=relay::PlayerSessionLease;
    Ip::set_session_limit(3);
    std::array<std::shared_ptr<Ip>,24> leases;std::array<std::shared_ptr<Player>,24> players;
    std::array<std::thread,24> threads;std::atomic<bool> go{false};
    for(std::size_t i=0;i<24;++i)threads[i]=std::thread([&,i]{while(!go.load())std::this_thread::yield();leases[i]=Ip::acquire("shared",Ip::Kind::Session);players[i]=Player::acquire(77);});
    go=true;for(auto& t:threads)t.join();
    unsigned sessions=0,users=0;for(std::size_t i=0;i<24;++i){sessions+=bool(leases[i]);users+=bool(players[i]);}
    require(sessions==3 && users==1);
    for(std::size_t i=0;i<24;++i)threads[i]=std::thread([&,i]{leases[i].reset();players[i].reset();});
    for(auto& t:threads)t.join();
    require(bool(Ip::acquire("shared",Ip::Kind::Session)) && bool(Player::acquire(77)));
    std::array<std::shared_ptr<Ip>,16> hs;
    for(auto& slot:hs){slot=Ip::acquire("shared",Ip::Kind::Handshake);require(bool(slot));}
    require(!Ip::acquire("shared",Ip::Kind::Handshake));
    auto session=Ip::acquire("shared",Ip::Kind::Session);require(bool(session));
    hs[0].reset();require(bool(Ip::acquire("shared",Ip::Kind::Handshake)));
    require(!Player::acquire(0) && !Player::acquire(-1));
    std::puts("root admission: 24-thread session/player caps, phase independence, final-owner release");
}
