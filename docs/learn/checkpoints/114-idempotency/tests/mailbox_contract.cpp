#include "net/hash_mailbox.h"
#include <atomic>
#include <thread>
#include <cstdio>
#include <cstdlib>
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"line %d: %s\n",__LINE__,#x);std::abort();}}while(false)
using namespace study_net;
int main(){
 HashMailbox<4,2> box;HashSample latest{99,77};HashComparison pair{99,77,88};
 CHECK(!box.latest_remote(latest)&&latest.tick==99&&latest.hash==77);
 CHECK(!box.poll(pair)&&pair.tick==99&&pair.local==77&&pair.remote==88);
 CHECK(box.record_remote(4,0)==HashPut::stored&&box.record_remote(8,8)==HashPut::stored);
 CHECK(box.latest_remote(latest)&&latest.tick==8&&latest.hash==8);
 CHECK(box.record_remote(12,12)==HashPut::too_far&&box.next_tick()==4);
 CHECK(box.record_local(8,8)==HashPut::stored&&!box.poll(pair)); // earliest pair missing
 CHECK(box.record_local(4,1)==HashPut::stored&&box.poll(pair));
 CHECK(pair.tick==4&&pair.local==1&&pair.remote==0); // mismatch is still consumed
 CHECK(box.poll(pair)&&pair.tick==8&&pair.local==pair.remote&&!box.poll(pair));
 CHECK(box.record_remote(8,9)==HashPut::stale&&box.latest_remote(latest)&&latest.tick==8&&latest.hash==8);
 CHECK(box.record_remote(12,4)==HashPut::stored&&box.record_remote(12,4)==HashPut::duplicate);
 CHECK(box.record_remote(12,5)==HashPut::conflict&&box.record_remote(13,4)==HashPut::invalid);
 CHECK(box.latest_remote(latest)&&latest.tick==12&&latest.hash==4);
 CHECK(box.record_remote(0,0)==HashPut::invalid);
 box.clear();CHECK(box.next_tick()==4&&!box.latest_remote(latest)&&latest.tick==12);
 // Latest means last accepted arrival, not highest tick; pair ordering is independent.
 CHECK(box.record_remote(8,88)==HashPut::stored&&box.record_remote(4,44)==HashPut::stored);
 CHECK(box.latest_remote(latest)&&latest.tick==4&&latest.hash==44);
 CHECK(box.record_local(4,44)==HashPut::stored&&box.poll(pair)&&pair.tick==4);
 CHECK(box.record_local(8,88)==HashPut::stored&&box.poll(pair)&&pair.tick==8);
 HashMailbox<UINT32_MAX,1> end;CHECK(end.record_remote(UINT32_MAX,0)==HashPut::stored);
 CHECK(end.record_local(UINT32_MAX,0)==HashPut::stored&&end.poll(pair)&&pair.tick==UINT32_MAX);
 CHECK(end.next_tick()==2ULL*UINT32_MAX&&end.record_remote(UINT32_MAX,0)==HashPut::exhausted&&!end.poll(pair));
 HashMailbox<4,8> shared;constexpr unsigned n=20000;
 auto value=[](unsigned t){return std::uint64_t(t)^0x123456789abcdef0ULL;};
 std::atomic_bool done{false};std::atomic<unsigned> observations{0};
 auto producer=[&](bool remote){for(unsigned i=1;i<=n;++i){HashPut result;
  do{result=remote?shared.record_remote(i*4,value(i*4)):shared.record_local(i*4,value(i*4));if(result==HashPut::too_far)std::this_thread::yield();}while(result==HashPut::too_far);
  CHECK(result==HashPut::stored);
 }};
 std::thread local(producer,false),remote(producer,true);
 std::thread observer([&]{while(!done.load()){HashSample sample;if(shared.latest_remote(sample)){CHECK(sample.hash==value(sample.tick));++observations;}std::this_thread::yield();}});
 for(unsigned i=1;i<=n;++i){HashComparison result;while(!shared.poll(result))std::this_thread::yield();CHECK(result.tick==i*4&&result.local==value(i*4)&&result.remote==result.local);}
 local.join();remote.join();done=true;observer.join();CHECK(shared.next_tick()==4ULL*(n+1));
 CHECK(shared.latest_remote(latest)&&latest.tick==n*4&&latest.hash==value(n*4));
 std::printf("mailbox: ordering/limits/duplicates/output preservation/exhaustion; 20000 concurrent comparisons, %u consistent observations\n",observations.load());
}
