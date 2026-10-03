// A controlled TCP peer drives each real Session receive phase. No private access.
#include "net/session.h"
#include <chrono>
#include <thread>
#include <cstdio>
#include <cstdlib>
#include <string>
int main(int argc,char** argv) {
    if(argc!=3) return 2;
    if(!net::net_init()) return 2;
    const std::string mode=argv[1];const auto port=static_cast<uint16_t>(std::stoul(argv[2]));
    net::Session session;bool started=false;
    if(mode=="io") started=session.Connect("127.0.0.1",port);
    else if(mode=="room") started=session.RoomCreate("127.0.0.1",port);
    else started=session.QueueJoin("127.0.0.1",port);
    if(!started) return 3;
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);
    bool announced=false;
    while(!session.hasFailed() && std::chrono::steady_clock::now()<deadline) {
        if(mode=="lobby" && session.isQueueMatched() && !announced) {
            std::puts("MATCHED");std::fflush(stdout);announced=true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    const bool ok=session.hasFailed() && !session.isConnected() && !session.isReady() &&
                  session.linkStatus()==net::LinkStatus::Lost;
    // hasFailed may publish just before the room-specific state update.
    while(mode=="room" && session.roomState()!=net::RoomState::Failed && std::chrono::steady_clock::now()<deadline)
        std::this_thread::yield();
    const bool roomOK=mode!="room" || session.roomState()==net::RoomState::Failed;
    session.Close();
    if(!ok || !roomOK) return 1;
    std::puts("FAILED_CLOSED");
}
