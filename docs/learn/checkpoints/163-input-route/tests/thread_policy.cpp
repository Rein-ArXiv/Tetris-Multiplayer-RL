// Controlled socket attempts exercise the real ThreadLink loop. These are
// adapter substitutes, not evidence of any particular kernel send behavior.
#include "net/thread_link.h"
#include "net/send_socket.h"
#include "net/receive_socket.h"
#include "net/stream.h"
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <thread>
#include <vector>
#define CHECK(x) do { if (!(x)) {std::fprintf(stderr,"policy failure line %d\n",__LINE__);std::abort();} } while(false)
namespace {
int mode=0;unsigned calls=0;bool half_closed=false;
std::vector<std::uint8_t> bytes;
}
namespace study_net {
bool set_nonblocking(Socket&,bool enabled,int& error) noexcept {error=0;return enabled;}
SendAttempt try_send(Socket&,const std::uint8_t* data,std::size_t size) noexcept {
    ++calls;
    if(mode==2)return {SendState::would_block,0,0};
    if(mode==1 && bytes.size()==3)return {SendState::error,0,123};
    if(calls==1)return {SendState::interrupted,0,0};
    if(calls==2)return {SendState::would_block,0,0};
    CHECK(size>0);bytes.push_back(data[0]);return {SendState::progress,1,0};
}
ReceiveAttempt try_receive(Socket&,std::uint8_t*,std::size_t) noexcept {return {ReceiveState::eof,0,0};}
bool shutdown_send(Socket&,int& error) noexcept {half_closed=true;error=0;return true;}
}
int main(){using namespace study_net;
 for(mode=0;mode<3;++mode){calls=0;half_closed=false;bytes.clear();bytes.reserve(16);
  LinkReport result{LinkEnd::cancelled};
  {
   ThreadLink link(Socket{});Frame a;a.type=7;a.size=2;a.payload[0]=9;a.payload[1]=10;
   Frame b;b.type=8;
   CHECK(link.send(a)==QueuePut::stored);CHECK(link.send(b)==QueuePut::stored);link.finish_sending();
   auto limit=std::chrono::steady_clock::now()+std::chrono::seconds(8);bool done=false;
   while(std::chrono::steady_clock::now()<limit){if(auto r=link.report()){result=*r;done=true;break;}std::this_thread::sleep_for(std::chrono::milliseconds(1));}
   CHECK(done);
  } // join before inspecting worker-written fixture state
  if(mode==0){CHECK(result.end==LinkEnd::complete && half_closed);CHECK(bytes==std::vector<std::uint8_t>({3,0,7,9,10,1,0,8}));}
  if(mode==1){CHECK(result.end==LinkEnd::io_error && result.error==123 && !half_closed);CHECK(bytes==std::vector<std::uint8_t>({3,0,7}));}
  if(mode==2){CHECK(result.end==LinkEnd::send_timeout && bytes.empty() && !half_closed);}
 }
 std::puts("worker policy: 1-byte sends/interruption/would-block/FIFO/partial error/deadline passed");
}
