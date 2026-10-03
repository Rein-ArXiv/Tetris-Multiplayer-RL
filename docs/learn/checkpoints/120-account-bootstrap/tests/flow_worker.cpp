// Adapter-controlled partial send tests the real ThreadLink accounting.
#include "net/thread_link.h"
#include "net/send_socket.h"
#include "net/receive_socket.h"
#include "net/stream.h"
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <thread>
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"flow worker line %d\n",__LINE__);std::abort();}}while(false)
namespace {std::atomic_bool partial{false},allow{false};}
namespace study_net {
bool set_nonblocking(Socket&,bool enabled,int&error)noexcept{error=0;return enabled;}
SendAttempt try_send(Socket&,const std::uint8_t*,std::size_t n)noexcept{
 if(!partial.exchange(true))return {SendState::progress,1,0};
 if(!allow.load())return {SendState::would_block,0,0};
 return {SendState::progress,n,0};
}
ReceiveAttempt try_receive(Socket&,std::uint8_t*,std::size_t)noexcept{return {ReceiveState::eof,0,0};}
bool shutdown_send(Socket&,int&error)noexcept{error=0;return true;}
}
int main(){using namespace study_net;ThreadLink link(Socket{});Frame frame;frame.type=40;frame.size=32;
 for(unsigned i=0;i<3;++i)CHECK(link.send(frame)==QueuePut::stored);
 auto until=std::chrono::steady_clock::now()+std::chrono::seconds(3);
 while(!partial.load()&&std::chrono::steady_clock::now()<until)std::this_thread::yield();
 CHECK(partial.load());
 auto stats=link.send_stats();CHECK(stats.bytes==105&&stats.active_bytes==35&&stats.paused&&stats.queued==2);CHECK(link.send(frame)==QueuePut::full);
 link.finish_sending();allow=true;
 while(std::chrono::steady_clock::now()<until){if(auto report=link.report()){CHECK(report->end==LinkEnd::complete);CHECK(link.send_stats().bytes==0);std::puts("real worker: dequeue/partial send retains charge, full completion releases it");return 0;}std::this_thread::sleep_for(std::chrono::milliseconds(1));}
 CHECK(false);
}
