#include "net/framing.h"
#include "net/socket.h"
#include <cstdio>
#include <string>
#include <vector>
#define RLOG_WARN(x) do{}while(false)
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"line %d: %s\n",__LINE__,#x);return 1;}}while(false)
int continued=0;
void initial(std::vector<uint8_t> stream){net::TcpSocket sock;
#include "initial_parse.inc"
 ++continued;
}
void room(std::vector<uint8_t> stream){do{
#include "room_parse.inc"
 ++continued;
}while(false);}
#include "residual.inc"
int main(int argc,char**argv){
 if(argc!=2)return 2;
 const bool first=std::string(argv[1])=="first";
 auto enter=first?initial:room;
 auto valid=net::build_frame(net::MsgType::QUEUE_JOIN,{0});auto mixed=valid;mixed.push_back(255);mixed.push_back(255);
 enter(mixed);CHECK(continued==0);
 enter({255,255});CHECK(continued==0);
 enter(valid);CHECK(continued==1);
 valid.push_back(1);enter(valid);CHECK(continued==2);
 std::vector<net::Frame> frames={{net::MsgType::QUEUE_JOIN,{0}},{net::MsgType::CHAT,{0,1,2}}};
 auto want=net::build_frame(net::MsgType::CHAT,{0,1,2});want.push_back(1);
 CHECK(residual_stream(frames,1,{1})==want);CHECK(residual_stream(frames,2,{1})==std::vector<uint8_t>{1});
 std::printf("actual %s parse guard: fatal failure stops dispatch; valid/partial input and residual order preserved\n",argv[1]);
}
