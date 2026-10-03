"""Real relay methods with controlled send/receive and allocation faults."""
from pathlib import Path
import subprocess,sys
from check_learning_text_layout import run
R=Path(__file__).resolve().parents[1];OUT=R/'out/learning-checkpoints/132-root-backpressure'
PRE=r'''
#include "server/byte_budget.h"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <new>
#include <stdexcept>
#include <string>
#include <vector>
#define RLOG_WARN(...) ((void)0)
#define RLOG_DEBUG(...) ((void)0)
bool fail_alloc=false;
void*operator new(std::size_t n){if(fail_alloc){fail_alloc=false;throw std::bad_alloc();}if(auto p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void operator delete(void*p)noexcept{std::free(p);}void operator delete(void*p,std::size_t)noexcept{std::free(p);}
using relay::try_reserve_bytes;
using Clock=std::chrono::steady_clock;using TimePoint=Clock::time_point;
constexpr size_t kSendHighWater=4,kSendLowWater=2,kSendHardCap=8,kMaxLobbyBufBytes=64,kMaxBytesPerSecond=64;
constexpr auto kMaxPauseDuration=std::chrono::seconds(16);
size_t g_tx_budget=64;std::atomic<size_t>g_tx_total{0},g_tx_peak{0},g_reject_tx_budget{0};
namespace net {
constexpr unsigned kRead=1,kWrite=2;
struct Socket{int mode=0;size_t accepted=0;int reads=0;};enum class RejectReason{TxBudget};
bool tcp_send_some(Socket&s,const void*,size_t n,size_t&out){out=0;if(s.mode<0)return false;out=std::min(n,s.accepted);return true;}
bool tcp_recv_some(Socket&s,std::vector<uint8_t>&rx){++s.reads;rx.push_back(7);return true;}
}
enum class Stage{FirstFrame,Auth,Queued,Room,Lobby,Forward,Dead};
struct Conn{int fd=1;net::Socket sock;Stage stage=Stage::Forward;std::vector<uint8_t>tx,rx;bool read_paused=false,want_write=false;size_t rate_tokens=100,pause_credit=0;TimePoint last_activity{},paused_since{},tx_drained_at{},rate_refilled_at{};int rate_carry=0;Conn*peer=nullptr;};
struct Timers{int arms=0;TimePoint due{};void arm(Conn*,TimePoint when){++arms;due=when;}};
struct Reactor{bool fail=false;unsigned interest=0;bool modify(int,unsigned i,void*){interest=i;return !fail;}};
struct Fixture{
 std::unique_ptr<Reactor>reactor_=std::make_unique<Reactor>();
 Timers timers_;int handled=0;
 void close_conn(Conn*c,const char*){if(!c||c->stage==Stage::Dead)return;c->stage=Stage::Dead;release_tx(c);}
 auto idle_timeout(){return std::chrono::seconds(15);}
 void charge_lobby_no_shows(Conn*){}
 void refill_tokens(Conn*,TimePoint){}
 Conn*feeder_of(Conn*c){return c->peer;}
 std::string ident_of(Conn*){return {};}
 std::vector<uint8_t>build_reject(net::RejectReason,const char*){return {9,9,9};}
 void on_first_frame(Conn*){++handled;}void on_room(Conn*){++handled;}void on_lobby(Conn*){++handled;}void on_forward(Conn*){++handled;}void on_queued(Conn*){++handled;}
'''
POST=r'''
};
void check(bool ok){if(!ok)throw std::runtime_error("backpressure invariant");}
int main(int argc,char**argv){if(argc!=2)return 2;try{
 Fixture f;Conn a,b;a.peer=&b;b.peer=&a;const uint8_t data[10]={1,2,3,4,5,6,7,8,9,10};const std::string test=argv[1];
 if(test=="send-error"){b.sock.mode=-1;check(!f.queue_send(&b,data,3));check(b.stage==Stage::Dead);}
 else if(test=="global"){g_tx_budget=4;check(f.queue_send(&b,data,3));check(!f.queue_send(&b,data,2));check(g_tx_peak<=4&&g_tx_total==0&&b.stage==Stage::Dead);}
 else if(test=="local"){check(!f.queue_send(&b,data,9));check(g_tx_peak<=8&&g_tx_total==0);}
 else if(test=="allocation"){fail_alloc=true;check(!f.queue_send(&b,data,5));check(b.stage==Stage::Dead&&g_tx_total==0);}
 else if(test=="interest-failure"){f.reactor_->fail=true;check(!f.queue_send(&b,data,3));check(b.stage==Stage::Dead&&g_tx_total==0);}
 else if(test=="read-interest-failure"){a.read_paused=true;a.paused_since=Clock::now()-std::chrono::seconds(1);f.reactor_->fail=true;f.pause_peer_read(&b,false);check(a.stage==Stage::Dead&&f.timers_.arms==0);}
 else if(test=="stale-read"){a.read_paused=true;f.on_readable(&a);check(a.sock.reads==0&&f.handled==0);}
 else if(test=="paused-error"){a.read_paused=true;
#ifdef HAS_ERROR_ARG
 f.on_readable(&a,true);
#else
 f.on_readable(&a);
#endif
 check(a.stage==Stage::Dead&&a.sock.reads==0);}
 else if(test=="hysteresis"){check(f.queue_send(&b,data,5));check(a.read_paused);b.sock.accepted=2;f.on_writable(&b);check(a.read_paused&&b.tx.size()==3);b.sock.accepted=1;f.on_writable(&b);check(!a.read_paused&&b.tx.size()==2);}
 else if(test=="dual-progress"){a.read_paused=b.read_paused=true;a.paused_since=Clock::now()-std::chrono::seconds(60);b.tx_drained_at=Clock::now();f.on_timeout(&a);check(a.stage!=Stage::Dead&&b.stage!=Stage::Dead&&f.timers_.arms==1);}
 else if(test=="dual-stall"){a.read_paused=b.read_paused=true;a.paused_since=Clock::now()-std::chrono::seconds(60);b.tx_drained_at=a.paused_since;f.on_timeout(&a);check(b.stage==Stage::Dead);}
 else if(test=="fifo"){check(f.queue_send(&b,data,5));check(f.queue_send(&b,data+5,3));b.sock.accepted=3;f.on_writable(&b);check(b.tx==std::vector<uint8_t>({4,5,6,7,8})&&g_tx_total==5);f.close_conn(&b,"discard");check(g_tx_total==0);}
 else return 2;
 f.close_conn(&a,"cleanup");f.close_conn(&b,"cleanup");check(g_tx_total==0);std::puts("actual relay send/queue/read/deadline contract passed");
 }catch(const std::exception&e){std::fprintf(stderr,"%s\n",e.what());return 1;}}
'''
def extract(s):
 def cut(a,b):i=s.index(a);return s[i:s.index(b,i)]
 start='    enum class TxAppend'if'    enum class TxAppend'in s else'    bool queue_send('
 return cut(start,'    // 대기 왕복')+cut('    void reject_conn(','    // dst 로 흘려보내는 쪽.')+cut('    bool arm_write('if'    bool arm_write('in s else'    void arm_write(','    // 흐른 시간만큼')+cut('    static void grant_pause_credit(','    void on_writable(')+cut('    void on_writable(','    void on_first_frame(')+cut('    void on_timeout(','    // ── finalize')
def main():
 OUT.mkdir(parents=True,exist_ok=True);before='--before'in sys.argv
 p=R/('out/learning-jobs/132-relay-before.cpp'if before else'server/reactor_relay.cpp');src=OUT/('before.cpp'if before else'after.cpp');src.write_text(('#define HAS_ERROR_ARG\n'if not before else'')+PRE+extract(p.read_text())+POST);exe=src.with_suffix('')
 run(['c++','-std=c++17','-I'+str(R),'-fsanitize=address,undefined','-fno-sanitize-recover=all',str(src),'-o',str(exe)])
 for scenario in ['send-error','global','local','allocation','interest-failure','read-interest-failure','stale-read','paused-error','hysteresis','dual-progress','dual-stall','fifo']:
  p=subprocess.run([str(exe),scenario],capture_output=True,text=True,timeout=10);(OUT/(exe.name+'-'+scenario+'.log')).write_text(p.stdout+p.stderr)
  expected_failure=before and scenario!='fifo'
  assert (p.returncode!=0 if expected_failure else p.returncode==0),(scenario,p.stdout,p.stderr)
  print(('before exposed'if expected_failure else'passed'),scenario,flush=True)
if __name__=='__main__':main()
