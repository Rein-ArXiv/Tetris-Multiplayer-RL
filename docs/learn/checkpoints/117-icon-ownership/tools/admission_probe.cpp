#include "net/first_admission.h"
#include "net/receive_socket.h"
#include "net/send_socket.h"
#include "net/stream.h"
#include "net/round_play.h"
#include <chrono>
#include <thread>
#include <vector>
#include <cstdio>
#include <cstdlib>
#include <cstring>
using namespace study_net;
using Clock=std::chrono::steady_clock;
#define REQUIRE(x) do{if(!(x)){std::fprintf(stderr,"line %d: %s\n",__LINE__,#x);std::exit(1);}}while(false)
void append_frame(std::vector<std::uint8_t>& bytes,const Frame& frame){EncodedFrame e;REQUIRE(encode_frame(frame,e));bytes.insert(bytes.end(),e.bytes.begin(),e.bytes.begin()+e.size);}
void write_fixture(Socket& socket,const std::vector<std::uint8_t>& bytes){
 std::size_t sent=0;
 while(sent<bytes.size()){auto r=send_some(socket,bytes.data()+sent,bytes.size()-sent);REQUIRE(r.status==StreamStatus::progress);sent+=r.count;}
}
std::uint64_t elapsed_ms(Clock::time_point start){return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now()-start).count());}
Frame read_next(Socket& socket,FrameParser& parser){
 const auto stop=Clock::now()+std::chrono::seconds(1);
 for(;;){
  Frame frame;const auto parsed=parser.next(frame);
  if(parsed==ParseStatus::frame)return frame;
  REQUIRE(parsed==ParseStatus::need_more&&Clock::now()<stop);
  std::uint8_t bytes[16];const auto read=try_receive(socket,bytes,sizeof(bytes));
  if(read.state==ReceiveState::progress)REQUIRE(parser.append(bytes,read.count));
  else {REQUIRE(read.state==ReceiveState::would_block||read.state==ReceiveState::interrupted);std::this_thread::sleep_for(std::chrono::milliseconds(1));}
 }
}
int main(int argc,char**argv){
 if(argc==2&&std::strcmp(argv[1],"--help")==0){std::puts("admission_probe: controlled loopback first command and parser/socket handoff; no authentication");return 0;}
 if(argc!=1)return 2;
 Runtime runtime;REQUIRE(runtime.ready());int error=0;std::uint16_t port=0;
 auto listener=listen_loopback(0,port,error);REQUIRE(listener.valid());
 auto client=connect_loopback(port,error);REQUIRE(client.valid());
 auto accepted=accept_one(listener,error);REQUIRE(accepted.valid());listener.reset();
 REQUIRE(set_nonblocking(accepted,true,error));
 Frame command;command.type=50;command.size=2;command.payload={1,0};
 Frame follow;follow.type=60;follow.size=1;follow.payload={9};
 Frame final;final.type=61;final.size=1;final.payload={8};
 std::vector<std::uint8_t> prefix;append_frame(prefix,command);append_frame(prefix,follow);
 EncodedFrame final_bytes;REQUIRE(encode_frame(final,final_bytes));prefix.insert(prefix.end(),final_bytes.bytes.begin(),final_bytes.bytes.begin()+2);
 write_fixture(client,prefix);
 const auto start=Clock::now();auto admission=*FirstAdmission::create(0,1000);
 for(;;){
  const auto state=admission.poll(elapsed_ms(start));
  if(state==AdmissionState::routed)break;
  REQUIRE(state==AdmissionState::waiting);
  std::uint8_t bytes[16];const auto read=try_receive(accepted,bytes,admission.read_capacity());
  if(read.state==ReceiveState::progress){const auto next=admission.feed(bytes,read.count,elapsed_ms(start));REQUIRE(next==AdmissionState::waiting||next==AdmissionState::routed);}
  else {REQUIRE(read.state==ReceiveState::would_block||read.state==ReceiveState::interrupted);std::this_thread::sleep_for(std::chrono::milliseconds(1));}
 }
 AdmissionHandoff handoff;REQUIRE(admission.take(handoff)&&handoff.request.route==AdmissionRoute::queue);
 Socket next_owner=std::move(accepted);REQUIRE(!accepted.valid()&&next_owner.valid());
 std::vector<std::uint8_t> suffix(final_bytes.bytes.begin()+2,final_bytes.bytes.begin()+final_bytes.size);
 RoundBatch input;input.round=1;input.inputs.count=1;input.inputs.masks[0]=study_input::drop;
 Frame input_frame;REQUIRE(encode_round_input(input,input_frame));append_frame(suffix,input_frame);write_fixture(client,suffix);
 REQUIRE(shutdown_send(client,error));
 const auto first=read_next(next_owner,handoff.parser);REQUIRE(first.type==60&&first.payload[0]==9);
 const auto second=read_next(next_owner,handoff.parser);REQUIRE(second.type==61&&second.payload[0]==8);
 const auto received=read_next(next_owner,handoff.parser);
 const auto board=study_round::Round::create_seeded(study_grid::Grid{},77);REQUIRE(board);
 RoundPlay peer;REQUIRE(peer.prepare(1,study_combat::Duel(*board,*board),Side::peer,0)&&peer.start());
 Frame own;REQUIRE(peer.capture(0,own).input==Put::stored&&peer.receive(received).input==Put::stored&&peer.advance()==Advance::advanced);
 REQUIRE(peer.game()->next_tick()==1&&handoff.parser.pending_bytes()==0);
 std::puts("routed queue request; socket and parser handed to one next owner");
 std::puts("complete and partial follow-up bytes preserved; real RoundPlay consumed tick 0");
 std::puts("route selection is not authentication or matching");
}
