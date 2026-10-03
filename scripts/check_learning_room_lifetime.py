"""Actual room/close methods: send failure may synchronously erase the Room."""
from pathlib import Path
import subprocess,sys
from check_learning_text_layout import run
ROOT=Path(__file__).resolve().parents[1];OUT=ROOT/'out/learning-checkpoints/131-room-lifetime'
PRE=r'''
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <deque>
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#define RLOG_INFO(...) ((void)0)
#define RLOG_WARN(...) ((void)0)
#define RLOG_DEBUG(...) ((void)0)
using Clock=std::chrono::steady_clock;using TimePoint=Clock::time_point;
constexpr auto kRoomGuestWait=std::chrono::minutes(15),kRoomReadyWait=std::chrono::minutes(1);
constexpr uint8_t kStatusWaiting=0,kStatusFull=1,kStatusNotFound=2,kStatusGoneFull=3;
std::atomic<int> g_conn_count{0},g_reject_room_guess{0};
namespace net {
struct Socket{bool open=true;};inline void tcp_close(Socket&s){s.open=false;}
enum class MsgType{READY,ROOM_LEAVE,CHAT};enum class RejectReason{RoomGuessLimit};
struct Frame{MsgType type;std::vector<uint8_t>payload;};
bool parse_frames(std::vector<uint8_t>&rx,std::vector<Frame>&out){for(auto b:rx)out.push_back({b==1?MsgType::READY:MsgType::CHAT,{1}});rx.clear();return true;}
std::vector<uint8_t> build_frame(MsgType,const std::vector<uint8_t>&p){return p;}
}
enum class Stage{Room,Lobby,Forward,Dead};
struct Room;struct Channel;
struct Conn{
 net::Socket sock;int fd=0;uint32_t id=0;Stage stage=Stage::Room;
 bool is_host=false,is_a=false,ready=false;Room*room=nullptr;Channel*ch=nullptr;
 std::string ip="test",join_code="ABCDE",icon="default";int player_id=0;
 std::vector<uint8_t>rx,tx;
 std::shared_ptr<int>handshake_slot,session_slot,lease;
 std::shared_ptr<std::atomic<bool>>auth_cancel;
};
struct Room{std::string code;Conn*host=nullptr;Conn*guest=nullptr;};
struct Channel{Conn*a=nullptr,*b=nullptr;int disconnect_side=0;bool finalize_inflight=false,close_survivor_pending=false;uint64_t seed=1;std::string match_uuid="match";};
struct RoomGuessBudget{static bool charge(const std::string&,TimePoint){return true;}};
struct Reactor{void remove(int){}};
struct Timers{
 std::unordered_map<Conn*,TimePoint> live;
 void cancel(Conn*c){live.erase(c);}
 void arm(Conn*c,TimePoint at){if(!c||c->stage==Stage::Dead)throw std::runtime_error("armed dead connection");live[c]=at;}
};
struct Fixture{
 std::unique_ptr<Reactor>reactor_=std::make_unique<Reactor>();Timers timers_;
 std::unordered_map<std::string,std::unique_ptr<Room>>rooms_;
 std::unordered_set<uint32_t>pending_auth_;std::deque<Conn*>queue_;std::vector<Conn*>dying_;
 std::vector<std::unique_ptr<Conn>>owned;bool fail[3]={false,false,false};bool exhausted=false,close_peer_during_pause=false;int pause_depth=0;Channel match;
 bool alive(Conn*c){return c&&c->stage!=Stage::Dead;}
 void pause_peer_read(Conn*c,bool){if(close_peer_during_pause&&c->room){if(++pause_depth>4)throw std::runtime_error("close reentered before dead mark");auto p=c->is_host?c->room->guest:c->room->host;close_conn(p,"injected interest failure");--pause_depth;}}void release_pending_auth_ip(uint32_t){}
 static void release_tx(Conn*c){c->tx.clear();}
 void on_channel_peer_lost(Channel*){}void close_channel_survivor(Channel*,const char*){}
 bool queue_send(Conn*c,const uint8_t*,std::size_t){if(fail[c->id]){close_conn(c,"injected send failure");return false;}return true;}
 void send_room_info(Conn*c,const std::string&code,uint8_t,uint8_t){std::string saved=code;queue_send(c,nullptr,saved.size());}
 void reject_conn(Conn*c,net::RejectReason,const char*,const char*why){close_conn(c,why);}
 std::string generate_code(){return "ABCDE";}
 void on_lobby(Conn*){}
 auto lobby_timeout(){return std::chrono::seconds(30);}
 Channel*make_channel(Conn*a,Conn*b){if(exhausted){close_conn(a,"ID exhausted");close_conn(b,"ID exhausted");return nullptr;}match.a=a;match.b=b;return &match;}
 bool send_match_found(Conn*,uint8_t,uint64_t,const std::string&,const std::string&,const std::string&){return true;}
 void begin_forwarding(Channel*c){c->a->stage=Stage::Forward;c->b->stage=Stage::Forward;}
 Conn* add(uint32_t id){auto c=std::make_unique<Conn>();c->id=id;auto p=c.get();owned.push_back(std::move(c));++g_conn_count;return p;}
 void room(Conn*h,Conn*g=nullptr){auto p=std::make_unique<Room>();p->code="ABCDE";p->host=h;p->guest=g;h->room=p.get();h->is_host=true;if(g)g->room=p.get();rooms_["ABCDE"]=std::move(p);}
'''
POST=r'''
};
int main(int argc,char**argv){
 if(argc!=2)return 2;
 try{
  Fixture f;auto h=f.add(1);auto g=f.add(2);const std::string scenario=argv[1];
  if(scenario=="close"){f.room(h,g);f.fail[1]=true;f.close_conn(g,"guest left");}
  else if(scenario=="join"){f.room(h);f.fail[1]=f.fail[2]=true;f.room_join(g);}
  else if(scenario=="frames"){f.room(h,g);f.fail[1]=f.fail[2]=true;g->rx={2,1};f.on_room(g);}
  else if(scenario=="create"){f.fail[1]=true;f.room_create(h);f.close_conn(g,"unused");}
  else if(scenario=="host_lost"){f.room(h);f.fail[1]=true;f.room_join(g);
    if(f.alive(h)||!f.alive(g)||!g->room||g->room->host||f.timers_.live.count(g)!=1||f.timers_.live.at(g)<Clock::now()+std::chrono::minutes(14))return 1;
    f.close_conn(g,"cleanup");}
  else if(scenario=="control_reentry"){f.room(h,g);f.close_peer_during_pause=true;f.close_conn(h,"start");}
  else if(scenario=="room_exhausted"){f.room(h,g);f.exhausted=true;f.start_room_match(h->room);}
  else if(scenario=="queue_exhausted"){f.exhausted=true;f.start_match(h,g);}
  else return 2;
  if(f.alive(h)||f.alive(g)||!f.rooms_.empty()||!f.timers_.live.empty()||g_conn_count!=0)return 1;
  std::puts("room callback lifetime passed");
 }catch(const std::exception&e){std::fprintf(stderr,"%s\n",e.what());return 1;}
}
'''
def methods(text):
 def cut(a,b):i=text.index(a);return text[i:text.index(b,i)]
 return cut('    void close_conn(','    void sweep()')+cut('    void room_create(','    // ── 큐 ')+cut('    void start_match(','    bool send_match_found(')
def main():
 OUT.mkdir(parents=True,exist_ok=True)
 before='--before'in sys.argv
 path=ROOT/(sys.argv[sys.argv.index('--source')+1]if'--source'in sys.argv else'out/learning-jobs/131-relay-before.cpp'if before else'server/reactor_relay.cpp')
 memory='--memory'in sys.argv
 assert not memory or before, '--memory isolates original UAF paths'
 prefix='before-memory'if memory else'before'if before else'after'
 pre=PRE.replace('if(!c||c->stage==Stage::Dead)throw std::runtime_error("armed dead connection");','')if memory else PRE
 src=OUT/(prefix+'.cpp');src.write_text(pre+methods(path.read_text())+POST);exe=src.with_suffix('')
 run(['c++','-std=c++17','-fsanitize=address,undefined','-fno-sanitize-recover=all',str(src),'-o',str(exe)])
 for scenario in ([sys.argv[sys.argv.index('--case')+1]]if'--case'in sys.argv else['close','join','frames','create','host_lost','room_exhausted','queue_exhausted','control_reentry']):
  p=subprocess.run([str(exe),scenario],capture_output=True,text=True,timeout=10)
  (OUT/(prefix+'-'+scenario+'.log')).write_text(p.stdout+p.stderr)
  if before:assert p.returncode!=0,scenario
  else:assert p.returncode==0,(scenario,p.stdout,p.stderr)
  print(('before reproduced'if before else'after passed'),scenario,flush=True)
if __name__=='__main__':main()
