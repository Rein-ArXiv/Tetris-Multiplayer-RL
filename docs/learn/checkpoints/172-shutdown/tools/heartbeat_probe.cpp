#include "net/thread_link.h"
#include <charconv>
#include <cstdio>
#include <cstring>
#include <chrono>
#include <thread>
int main(int argc,char**argv){using namespace study_net;
 if(argc==2&&std::strcmp(argv[1],"--help")==0){std::puts("heartbeat_probe listen PORT | connect PORT");return 0;}
 if(argc!=3)return 2;
 unsigned port=0;auto end=argv[2]+std::strlen(argv[2]);auto parsed=std::from_chars(argv[2],end,port);
 if(parsed.ec!=std::errc{}||parsed.ptr!=end||port>65535)return 2;
 bool host=std::strcmp(argv[1],"listen")==0;if(!host&&(std::strcmp(argv[1],"connect")!=0||port==0))return 2;
 Runtime rt;if(!rt.ready())return 1;int error=0;Socket socket;
 if(host){std::uint16_t bound=0;auto listener=listen_loopback(static_cast<std::uint16_t>(port),bound,error);if(!listener.valid())return 1;
  std::printf("LISTEN %u\n",unsigned(bound));std::fflush(stdout);socket=accept_one(listener,error);
 }else socket=connect_loopback(static_cast<std::uint16_t>(port),error);
 if(!socket.valid())return 1;
 ThreadLink link(std::move(socket),true,{40,150,350});
 const auto start=std::chrono::steady_clock::now();
 bool finishing=false, peer_done=false;
 while(std::chrono::steady_clock::now()-start<std::chrono::seconds(2)){
  if(!finishing && std::chrono::steady_clock::now()-start>=std::chrono::milliseconds(600)){
   Frame done;done.type=32;
   if(link.send(done)!=QueuePut::stored)return 1;
   link.finish_sending();finishing=true;
  }
  Frame frame;QueueGet got;
  while((got=link.receive(frame))==QueueGet::item){
   if(frame.type!=32 || frame.size!=0 || peer_done)return 1;
   peer_done=true;
  }
  if(auto result=link.report()){
   if(got!=QueueGet::closed)continue;
   auto count=link.confirmed_pongs();
   std::printf("END %d confirmations=%llu done=%d\n",int(result->end),static_cast<unsigned long long>(count),int(peer_done));
   return result->end==LinkEnd::complete && peer_done && finishing && count>=3?0:1;
  }
  std::this_thread::sleep_for(std::chrono::milliseconds(2));
 }
 std::puts("TIMEOUT");return 1;
}
