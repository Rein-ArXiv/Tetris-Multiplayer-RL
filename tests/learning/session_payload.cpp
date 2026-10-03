// Controlled peer sends INPUT/MATCH_RESULT then a HASH barrier on the same stream.
#include "net/session.h"
#include <chrono>
#include <thread>
#include <cstdio>
#include <string>
int main(int argc,char** argv) {
    if(argc!=3 || !net::net_init()) return 2;
    const std::string mode=argv[1];net::Session session;
    if(!session.Connect("127.0.0.1",static_cast<uint16_t>(std::stoul(argv[2])))) return 2;
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);
    uint32_t tick=0;uint64_t hash=0;
    while(std::chrono::steady_clock::now()<deadline) {
        if(session.GetLastRemoteHash(tick,hash) && tick==600 && hash==456) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    if(tick!=600 || hash!=456 || session.hasFailed()) return 1;
    if(mode=="good") {
        uint8_t mask=0;
        if(!session.GetRemoteInput(0,mask) || mask!=3 || !session.GetRemoteInput(1,mask) || mask!=16) return 1;
    } else if(mode=="result") {
        net::Session::MatchResult result;
        if(!session.GetMatchResult(result) || result.elo_before!=(-2147483647-1) || result.elo_after!=2147483647 || result.delta!=-1) return 1;
    } else {
        uint8_t mask=99;
        if(session.GetRemoteInput(0,mask) || session.GetRemoteInput(1,mask) || session.maxRemoteTick()!=0) return 1;
    }
    session.Close();std::puts("PAYLOAD_POLICY_OK");
}
