#include "net/session.h"
#include <chrono>
#include <thread>
#include <cstdio>
#include <cstdlib>
#define CHECK(x) do {if(!(x)){std::fprintf(stderr,"line %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
bool barrier(net::Session& s,uint32_t expected) {
 const auto end=std::chrono::steady_clock::now()+std::chrono::seconds(5);uint32_t tick=0;uint64_t hash=0;
 while(std::chrono::steady_clock::now()<end){if(s.GetLastRemoteHash(tick,hash)&&tick==expected)return true;std::this_thread::sleep_for(std::chrono::milliseconds(1));}return false;
}
int main(int argc,char** argv) {
 if(argc!=3)return 2;
 CHECK(net::net_init());
 const auto port=static_cast<uint16_t>(std::stoul(argv[2]));net::Session s;
 if(std::string(argv[1])=="host") {
  net::SeedParams config;config.seed=77;config.role=net::Role::Host;CHECK(s.Host(port,config));
  CHECK(barrier(s,600));CHECK(s.params().role==net::Role::Host);s.SendNewSeed(78);
  CHECK(barrier(s,1200));CHECK(s.params().role==net::Role::Host && s.params().seed==78);
 } else {
  CHECK(s.Connect("127.0.0.1",port));CHECK(barrier(s,600));
  auto config=s.params();CHECK(config.seed==77 && config.role==net::Role::Peer && s.isReady());
  s.SendHash(1200,0);CHECK(barrier(s,1800));config=s.params();
  CHECK(config.seed==77 && config.role==net::Role::Peer && config.start_tick==120 && config.input_delay==2 && s.isReady());
  s.SendHash(2400,0);
  // A valid subsequent SEED remains supported for practice rematches.
  CHECK(barrier(s,3000));CHECK(s.params().seed==79 && s.params().role==net::Role::Peer);
 }
 s.Close();std::puts("SEED_SESSION_OK");
}
