#include "net/hash_mailbox.h"
#include "net/framing.h"
#include <atomic>
#include <mutex>
#include <cstdio>
#include <cstdlib>
using namespace net;
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"line %d: %s\n",__LINE__,#x);return 1;}}while(false)
#define NET_WARN(x) do{}while(false)
struct Breakdown{uint64_t grid=0,currentBlock=0,nextBlock=0,rng=0,scoreFlags=0,combat=0;};
struct Sim{Breakdown StateHashBreakdown()const{return {};}};
struct Game{Sim sim;};
enum class AppMode{Net};
struct Session{
 HashExchange hashMailbox_;
 mutable std::mutex hashMu_;uint32_t lastHashTickRemote=0;uint64_t lastHashRemote=0;
 bool connectionFailed=false,quit=false;unsigned queued=0;
 void pushSend(std::vector<uint8_t>&&){++queued;}
 void SendHash(uint32_t,uint64_t);
 bool GetLastRemoteHash(uint32_t&,uint64_t&)const;
 bool PollHashComparison(uint32_t&,uint64_t&,uint64_t&);
 void handle(const Frame& f){switch(f.type){
#include "hash_receive.inc"
 default:break;
 }}
};
#include "hash_methods.inc"
Frame frame(uint32_t tick,uint64_t hash){Frame f;f.type=MsgType::HASH;le_write_u32(f.payload,tick);le_write_u64(f.payload,hash);return f;}
int main(){
 Session session;AppMode app=AppMode::Net;Game local,remote;Game*gameLocal=&local;Game*gameRemote=&remote;uint32_t simTick=1201;
#include "hash_storage.inc"
 auto compare=[&]{
#include "hash_compare.inc"
 };
 session.handle(frame(600,9));session.handle(frame(1200,22));
 uint32_t latest=0;uint64_t hash=0;CHECK(session.GetLastRemoteHash(latest,hash)&&latest==1200&&hash==22);
#ifdef BEFORE
 localHashRing[1]={600,11,true};localHashRing[2]={1200,22,true};
 compare();CHECK(desyncDetected&&desyncTick==600);
 (void)resetHashComparison;
#else
 compare();CHECK(!desyncDetected);
 session.SendHash(600,11);session.SendHash(1200,22);CHECK(session.queued==2&&!session.quit);
 compare();CHECK(desyncDetected&&desyncTick==600);
 desyncDetected=false;compare();CHECK(!desyncDetected);
 lastHashSentTick=1200;resetHashComparison();CHECK(lastHashSentTick==UINT32_MAX&&!desyncDetected&&desyncTick==0);
 Session duplicate;duplicate.handle(frame(600,0));duplicate.handle(frame(600,0));CHECK(!duplicate.quit);
 duplicate.handle(frame(600,1));CHECK(duplicate.quit&&duplicate.connectionFailed);
 for(unsigned mode=0;mode<3;++mode){Session bad;auto f=frame(mode==0?0:5400,7);if(mode==2)f.payload.pop_back();bad.handle(f);CHECK(bad.quit&&bad.connectionFailed);}
 Session bounded;for(unsigned i=1;i<=8;++i)bounded.SendHash(i*600,i);CHECK(!bounded.quit&&bounded.queued==8);bounded.SendHash(5400,9);CHECK(bounded.quit&&bounded.connectionFailed&&bounded.queued==8);
 session.hashMailbox_.clear();latest=99;hash=88;CHECK(!session.GetLastRemoteHash(latest,hash)&&latest==99&&hash==88);
 session.handle(frame(600,0));session.SendHash(600,0);uint64_t lh=1,rh=1;CHECK(session.PollHashComparison(latest,lh,rh)&&latest==600&&lh==0&&rh==0);
 std::puts("actual Session/main: retain earlier mismatch, deferred comparison, once-only consumption, reset, domain/conflict/capacity policy");
#endif
 (void)HASH_PERIOD_TICKS;(void)lastHashSentTick;
}
