#include "net/worker_group.h"
#include "net/first_admission.h"
#include "net/receive_socket.h"
#include "net/send_socket.h"
#include "net/stream.h"
#include "net/round_play.h"
#include <array>
#include <chrono>
#include <cstdio>
#include <stdexcept>
#include <thread>
#include <vector>
using namespace study_net;
using Clock=std::chrono::steady_clock;
void require(bool ok){if(!ok)throw std::runtime_error("worker probe contract");}
struct Result{AdmissionRoute route=AdmissionRoute::queue;std::uint64_t tick=0;};
std::uint64_t elapsed(Clock::time_point start){return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now()-start).count());}
Result serve(Socket socket){
 int error=0;require(set_nonblocking(socket,true,error));
 const auto start=Clock::now();auto admission=*FirstAdmission::create(0,1000);
 while(admission.poll(elapsed(start))==AdmissionState::waiting){
  std::uint8_t bytes[16];const auto read=try_receive(socket,bytes,admission.read_capacity());
  if(read.state==ReceiveState::progress){
   const auto state=admission.feed(bytes,read.count,elapsed(start));
   require(state==AdmissionState::waiting||state==AdmissionState::routed);
  }else{
   require(read.state==ReceiveState::would_block||read.state==ReceiveState::interrupted);
   std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
 }
 AdmissionHandoff handoff;require(admission.take(handoff));
 Frame input;
 for(;;){
  const auto parsed=handoff.parser.next(input);
  if(parsed==ParseStatus::frame)break;
  require(parsed==ParseStatus::need_more&&elapsed(start)<2000);
  std::uint8_t bytes[16];const auto read=try_receive(socket,bytes,sizeof(bytes));
  if(read.state==ReceiveState::progress)require(handoff.parser.append(bytes,read.count));
  else{require(read.state==ReceiveState::would_block||read.state==ReceiveState::interrupted);std::this_thread::sleep_for(std::chrono::milliseconds(1));}
 }
 auto board=study_round::Round::create_seeded(study_grid::Grid{},77);require(bool(board));
 RoundPlay peer;require(peer.prepare(1,study_combat::Duel(*board,*board),Side::peer,0)&&peer.start());
 Frame local;require(peer.capture(0,local).input==Put::stored&&peer.receive(input).input==Put::stored);
 require(peer.advance()==Advance::advanced);
 return {handoff.request.route,peer.game()->next_tick()};
}
void append(std::vector<std::uint8_t>& bytes,const Frame& f){EncodedFrame e;require(encode_frame(f,e));bytes.insert(bytes.end(),e.bytes.begin(),e.bytes.begin()+e.size);}
void send_fixture(Socket& socket,const std::vector<std::uint8_t>& bytes){
 std::size_t sent=0;
 while(sent<bytes.size()){auto r=send_some(socket,bytes.data()+sent,bytes.size()-sent);require(r.status==StreamStatus::progress&&r.count>0);sent+=r.count;}
}
int main(){
 try{
  Runtime runtime;require(runtime.ready());int error=0;std::uint16_t port=0;
  auto listener=listen_loopback(0,port,error);require(listener.valid());
  std::array<Socket,3> clients;
  // Results outlive workers. Distinct workers write distinct array elements.
  std::array<Result,3> results{};
  WorkerGroup workers{3};
  for(unsigned i=0;i<3;++i){
   clients[i]=connect_loopback(port,error);require(clients[i].valid());
   auto accepted=accept_one(listener,error);require(accepted.valid());
   Frame command;command.type=50;command.size=i==2?7:2;
   command.payload={static_cast<std::uint8_t>(i+1),static_cast<std::uint8_t>(i==2?5:0),'A','B','C','D','9'};
   RoundBatch batch;batch.round=1;batch.inputs.count=1;batch.inputs.masks[0]=study_input::drop;
   Frame input;require(encode_round_input(batch,input));
   std::vector<std::uint8_t> bytes;append(bytes,command);append(bytes,input);send_fixture(clients[i],bytes);
   require(workers.launch([socket=std::move(accepted),&results,i]()mutable{
    results[i]=serve(std::move(socket));
   }));
  }
  listener.reset();workers.stop_accepting();workers.wait();
  require(workers.active()==0&&workers.failed_tasks()==0);
  for(unsigned i=0;i<3;++i)require(results[i].route==static_cast<AdmissionRoute>(i+1)&&results[i].tick==1);
  std::puts("3 socket tasks: queue/create/join requests preserved; each real RoundPlay consumed tick 0");
  std::puts("closed admission -> drained task captures -> read results -> destroy borrowed state");
  return 0;
 }catch(const std::exception&e){std::fprintf(stderr,"%s\n",e.what());return 1;}
}
