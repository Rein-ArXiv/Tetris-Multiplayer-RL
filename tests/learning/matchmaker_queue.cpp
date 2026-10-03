#include "server/matchmaker.h"
#include "server/log.h"
#include "net/framing.h"
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <thread>
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"line %d: %s\n",__LINE__,#x);return 1;}}while(false)
struct Transport final:net::StreamTransport{
 std::mutex mu;std::condition_variable cv;bool closed=false;std::vector<uint8_t> bytes;
 bool alive()const override{return true;} // The queue observes EOF through receive.
 bool send(const void*,size_t)override{return true;}
 bool receive(std::vector<uint8_t>& out)override{
  std::lock_guard<std::mutex> lock(mu);if(closed)return false;
  out.insert(out.end(),bytes.begin(),bytes.end());bytes.clear();return true;
 }
 void close()override{std::lock_guard<std::mutex> lock(mu);closed=true;cv.notify_all();}
 bool wait_closed(){std::unique_lock<std::mutex> lock(mu);return cv.wait_for(lock,std::chrono::seconds(2),[&]{return closed;});}
};
relay::PlayerInfo peer(unsigned id,std::shared_ptr<Transport> t){
 relay::PlayerInfo p;p.conn_id=id;p.sock.transport=std::move(t);
 p.session_lease=relay::PlayerSessionLease::acquire(id);return p;
}
int main(int argc,char**argv){
 relay::set_log_level(relay::LogLevel::Warn);
 if(argc==2&&std::strcmp(argv[1],"late")==0){
  relay::Matchmaker queue;queue.shutdown();auto p=peer(1001,std::make_shared<Transport>());
  std::weak_ptr<relay::PlayerSessionLease> lease=p.session_lease;queue.enqueue(std::move(p));
  CHECK(lease.expired());CHECK(!queue.waitForPair());return 0;
 }
 if(argc==2&&std::strcmp(argv[1],"lone")==0){
  relay::Matchmaker queue;auto t=std::make_shared<Transport>();
  t->bytes=net::build_frame(net::MsgType::QUEUE_CANCEL,{});
  auto p=peer(1002,t);std::weak_ptr<relay::PlayerSessionLease> lease=p.session_lease;
  queue.enqueue(std::move(p));std::optional<relay::Match> result;
  std::thread consumer([&]{result=queue.waitForPair();});
  const bool removed_without_second_peer=t->wait_closed();
  queue.shutdown();consumer.join();
  CHECK(removed_without_second_peer);CHECK(!result&&lease.expired());return 0;
 }
 if(argc==2&&std::strcmp(argv[1],"full")==0){
  relay::Matchmaker queue;
  for(std::size_t i=0;i<relay::Matchmaker::kMaxWaiting;++i){relay::PlayerInfo filler;CHECK(queue.enqueue(std::move(filler)));}
  auto t=std::make_shared<Transport>();auto p=peer(1006,t);std::weak_ptr<relay::PlayerSessionLease> lease=p.session_lease;
  CHECK(!queue.enqueue(std::move(p)));CHECK(lease.expired()&&t->wait_closed());queue.shutdown();return 0;
 }
 relay::Matchmaker queue;auto a=std::make_shared<Transport>(),b=std::make_shared<Transport>(),c=std::make_shared<Transport>();
 b->bytes=net::build_frame(net::MsgType::QUEUE_CANCEL,{});
 queue.enqueue(peer(1003,a));queue.enqueue(peer(1004,b));queue.enqueue(peer(1005,c));
 auto pair=queue.waitForPair();CHECK(pair&&pair->a.conn_id==1003&&pair->b.conn_id==1005);
 CHECK(b->wait_closed());queue.shutdown();CHECK(!queue.waitForPair());
 std::puts("matchmaker: cancelled middle entry removed; surviving FIFO pair preserved");
}
