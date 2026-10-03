// Public-API regression: starts are rejected until owner Close joins old workers.
#include "net/session.h"
#include <cstdio>
#include <cstdlib>
#include <chrono>
#include <thread>
#define CHECK(x) do {if(!(x)){std::fprintf(stderr,"line %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
bool start(net::Session& s,int mode,uint16_t port) {
    switch(mode) {
      case 0:return s.Host(0,{});
      case 1:return s.Connect("127.0.0.1",port);
      case 2:return s.QueueJoin("127.0.0.1",port);
      case 3:return s.RoomCreate("127.0.0.1",port);
      default:return s.RoomJoin("127.0.0.1",port,"ABCDE");
    }
}
int main(int argc,char** argv) {
    if(argc!=4) return 2;
    const int mode=std::atoi(argv[1]);const bool failed=std::atoi(argv[3])!=0;
    const auto port=static_cast<uint16_t>(std::stoul(argv[2]));
    CHECK(net::net_init());
    {
        net::Session s;CHECK(start(s,mode,failed?0:port));
        if(failed) {
            const auto end=std::chrono::steady_clock::now()+std::chrono::seconds(3);
            while(!s.hasFailed() && std::chrono::steady_clock::now()<end) std::this_thread::sleep_for(std::chrono::milliseconds(1));
            CHECK(s.hasFailed());
        }
        // A completed worker is still joinable. None of these may reset flags,
        // start another worker, or overwrite a joinable std::thread.
        for(int next=0;next<5;++next) CHECK(!start(s,next,port));
        if(failed) CHECK(s.hasFailed());
        s.Close();s.Close();
        for(int round=0;round<2;++round) {
            CHECK(s.Connect("127.0.0.1",port));
            CHECK(!s.Connect("127.0.0.1",port));
            s.Close();CHECK(!s.isConnected() && !s.isReady());
        }
    }
    net::net_shutdown();std::puts("SESSION_LIFETIME_OK");
}
