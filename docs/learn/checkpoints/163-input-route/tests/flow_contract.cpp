#include "net/flow_queue.h"
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <thread>
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"flow line %d: %s\n",__LINE__,#x);std::abort();}}while(false)
using namespace study_net;
Frame frame(unsigned n,std::size_t payload=32){Frame f;f.type=40;f.size=payload;for(unsigned i=0;i<4;++i)f.payload[i]=static_cast<std::uint8_t>(n>>(i*8));return f;}
unsigned number(const Frame& f){unsigned n=0;for(unsigned i=0;i<4;++i)n|=unsigned(f.payload[i])<<(i*8);return n;}
void thresholds(){FlowQueue<8,140,105,70> q;Frame out=frame(99);CHECK(q.try_pop(out)==QueueGet::empty && number(out)==99);CHECK(!q.complete());
 for(unsigned i=0;i<3;++i)CHECK(q.try_push(frame(i))==QueuePut::stored);
 auto s=q.stats();CHECK(s.queued==3 && s.bytes==105 && s.paused);CHECK(q.try_push(frame(3))==QueuePut::full);
 CHECK(q.try_pop(out)==QueueGet::item && number(out)==0);s=q.stats();CHECK(s.queued==2 && s.active_bytes==35 && s.bytes==105 && s.paused);
 CHECK(q.try_pop(out)==QueueGet::empty && number(out)==0);CHECK(q.try_push(frame(3))==QueuePut::full);
 CHECK(q.complete());s=q.stats();CHECK(s.bytes==70&&!s.paused);CHECK(q.try_push(frame(3))==QueuePut::stored);q.close();q.close();CHECK(q.try_push(frame(4))==QueuePut::closed);
 for(unsigned i=1;i<=3;++i){CHECK(q.try_pop(out)==QueueGet::item&&number(out)==i);CHECK(q.complete());CHECK(!q.complete());}
 CHECK(q.try_pop(out)==QueueGet::closed);CHECK(q.stats().bytes==0);
 Frame bad;bad.size=33;CHECK(q.try_push(bad)==QueuePut::invalid);
 FlowQueue<2,140,105,70> count;CHECK(count.try_push(frame(0,4))==QueuePut::stored);CHECK(count.try_push(frame(1,4))==QueuePut::stored);
 CHECK(count.try_pop(out)==QueueGet::item);CHECK(count.try_push(frame(2,4))==QueuePut::full);CHECK(count.complete());CHECK(count.try_push(frame(2,4))==QueuePut::stored);
 FlowQueue<8,40,40,20> bytes;CHECK(bytes.try_push(frame(0,32))==QueuePut::stored);CHECK(bytes.try_push(frame(1,3))==QueuePut::full);CHECK(bytes.stats().bytes==35 && !bytes.stats().paused);
}
void model(){FlowQueue<8,140,105,70> q;std::deque<Frame> model;std::size_t active=0,total=0;bool pause=false,closed=false;unsigned rng=7;
 for(unsigned step=0;step<100000;++step){rng=rng*1664525u+1013904223u;unsigned op=rng%3;Frame f=frame(step,(rng>>8)%33);
  if(step==99000){q.close();closed=true;}
  if(op==0){auto expected=closed?QueuePut::closed:pause||model.size()+(active!=0)>=8||f.size+3>140-total?QueuePut::full:QueuePut::stored;
   CHECK(q.try_push(f)==expected);if(expected==QueuePut::stored){model.push_back(f);total+=f.size+3;if(total>=105)pause=true;}}
  else if(op==1){auto expected=active?QueueGet::empty:model.empty()?(closed?QueueGet::closed:QueueGet::empty):QueueGet::item;CHECK(q.try_pop(f)==expected);
   if(expected==QueueGet::item){CHECK(f.payload==model.front().payload&&f.size==model.front().size);active=f.size+3;model.pop_front();}}
  else {CHECK(q.complete()==(active!=0));if(active){total-=active;active=0;if(total<=70)pause=false;}}
  auto s=q.stats();CHECK(s.queued==model.size()&&s.bytes==total&&s.active_bytes==active&&s.paused==pause&&s.closed==closed);
 }
}
void threads(){FlowQueue<8,140,105,70> q;
 std::thread producer([&]{for(unsigned i=0;i<10000;++i){QueuePut p;while((p=q.try_push(frame(i)))==QueuePut::full)std::this_thread::yield();CHECK(p==QueuePut::stored);}q.close();});
 unsigned n=0;Frame out;for(;;){auto p=q.try_pop(out);if(p==QueueGet::closed)break;if(p==QueueGet::empty){std::this_thread::yield();continue;}CHECK(number(out)==n++);CHECK(q.complete());}
 producer.join();CHECK(n==10000&&q.stats().bytes==0);
}
int main(){thresholds();model();threads();std::puts("flow: thresholds/inflight/count/bytes/close/100000 model steps/10000 handoffs passed");}
