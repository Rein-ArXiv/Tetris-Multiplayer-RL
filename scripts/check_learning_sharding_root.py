"""Compile the actual RelayLoop with a controlled Reactor; inject transfer boundaries."""
from pathlib import Path
import subprocess,sys
from check_learning_text_layout import run
R=Path(__file__).resolve().parents[1];OUT=R/'out/learning-checkpoints/133-root-sharding'
TEST=r'''
#include <cstdio>
struct ControlledReactor : net::Reactor {
 std::unordered_map<net::NativeSocket,unsigned> interests;
 int fail_at=0,adds=0,wakes=0,remove_fail_at=0,removes=0;
 bool poll_failure=false;
 bool add(net::NativeSocket fd,unsigned flags,void*) override {
  ++adds;if(adds==fail_at)return false;return interests.emplace(fd,flags).second;
 }
 bool modify(net::NativeSocket fd,unsigned flags,void*) override {
  auto it=interests.find(fd);if(it==interests.end())return false;it->second=flags;return true;
 }
 bool remove(net::NativeSocket fd) override {if(++removes==remove_fail_at)return false;return interests.erase(fd)!=0;}
 int poll(std::vector<net::Event>&out,int) override {out.clear();return poll_failure?-1:0;}
 bool can_migrate_sockets()const override{return true;}
 void wake()override{++wakes;}
};
void check(bool ok,const char*why){if(!ok)throw std::runtime_error(why);}
ControlledReactor* setup(relay::RelayLoop&l){
 auto p=std::make_unique<ControlledReactor>();auto* raw=p.get();l.reactor_=std::move(p);
 l.offload_=std::make_unique<relay::Offload>(1,[]{});return raw;
}
relay::Channel* pair_in(relay::RelayLoop&l,unsigned id=1){
 using namespace relay;auto a=std::make_unique<Conn>(),b=std::make_unique<Conn>();auto ch=std::make_unique<Channel>();
 a->id=id*2;b->id=id*2+1;a->fd=id*2;b->fd=id*2+1;
 a->stage=b->stage=Stage::Lobby;a->ready=b->ready=true;a->is_a=true;b->is_a=false;
 a->tx={1,2};a->want_write=true;b->read_paused=true;b->paused_since=Clock::now();
 ch->match_id=id;ch->a=a.get();ch->b=b.get();a->ch=b->ch=ch.get();
 auto* result=ch.get();l.reactor_->add(a->fd,net::kRead|net::kWrite,a.get());l.reactor_->add(b->fd,0,b.get());
 l.timers_.arm(a.get(),Clock::now()+std::chrono::seconds(10));l.timers_.arm(b.get(),Clock::now()+std::chrono::seconds(10));
 l.conns_[a.get()]=std::move(a);l.conns_[b.get()]=std::move(b);l.channels_[id]=std::move(ch);
 g_conn_count+=2;g_match_count+=1;g_tx_total+=2;return result;
}
int main(int argc,char**argv){if(argc!=2)return 2;try{
 using namespace relay;set_log_level(LogLevel::Error); // fixture logs are not the oracle
 RelayLoop front(nullptr,"test"),shard(nullptr,"test");auto*fr=setup(front);auto*sr=setup(shard);
 const std::string which=argv[1];
 if(which=="poll-error"){
  g_running=true;sr->poll_failure=true;shard.run();check(!g_running,"poll failure left other loops running");front.shutdown();
 }else if(which=="full"){
  front.set_shards({&shard});for(unsigned i=1;i<=257;++i)front.begin_forwarding(pair_in(front,i));
  check(shard.inbox_.size()==256,"handoff bound");front.sweep();check(front.conns_.empty(),"rejected pair cleanup");
  check(g_conn_count==512&&g_match_count==256&&g_tx_total==512,"rejection accounting");
  front.shutdown();shard.shutdown();
 }else if(which=="remove-first"||which=="remove-second"){
  auto* ch=pair_in(front);front.set_shards({&shard});fr->remove_fail_at=which=="remove-first"?1:2;
  front.begin_forwarding(ch);check(shard.inbox_.empty(),"failed detach published pair");
  check(ch->summary_handled,"detach infrastructure failure can finalize");front.sweep();
  check(g_conn_count==0&&g_match_count==0&&g_tx_total==0,"detach cleanup");front.shutdown();shard.shutdown();
 }else if(which=="closed"){
  shard.shutdown();front.set_shards({&shard});front.begin_forwarding(pair_in(front));front.sweep();
  check(shard.inbox_.empty(),"closed inbox accepted pair");check(g_conn_count==0&&g_tx_total==0&&g_match_count==0,"closed rejection cleanup");front.shutdown();
 }else{
  auto*ch=pair_in(front);auto*a=ch->a;auto*b=ch->b;front.set_shards({&shard});front.begin_forwarding(ch);
  check(!front.alive(a)&&!front.alive(b)&&front.channels_.empty()&&fr->interests.empty(),"old owner still live");
  check(front.timers_.timeout_ms(Clock::now())<0,"old timers remain");
  if(which=="pending-shutdown"){shard.shutdown();}
  else {
   if(which=="add-first")sr->fail_at=1;else if(which=="add-second")sr->fail_at=2;
   shard.drain_inbox();
   if(which=="add-first"||which=="add-second"){
    check(!shard.alive(a)&&!shard.alive(b)&&sr->interests.empty(),"partial registration cleanup");
    check(ch->summary_handled,"infrastructure failure can finalize game");shard.sweep();
   }else if(which=="state"||which=="live-shutdown"){
    check(shard.alive(a)&&shard.alive(b)&&a->ch==ch&&b->ch==ch,"moved graph");
    check(sr->interests.at(a->fd)==(net::kRead|net::kWrite)&&sr->interests.at(b->fd)==0,"lost read/write state");
    check(a->tx==std::vector<uint8_t>({1,2})&&g_tx_total==2,"lost queue/budget");
    if(which=="state"){shard.abort_unstarted_match(ch,"test");shard.sweep();}
   }else return 2;
   shard.shutdown();
  }
  front.shutdown();
 }
 check(g_conn_count==0&&g_match_count==0&&g_tx_total==0,"shutdown accounting");
 std::puts("actual RelayLoop handoff/registration/shutdown contract passed");return 0;
 }catch(const std::exception&e){std::fprintf(stderr,"%s\n",e.what());return 1;}}
'''

def main():
 OUT.mkdir(parents=True,exist_ok=True);before='--before'in sys.argv
 source=(R/('out/learning-jobs/133-relay-before.cpp'if before else'server/reactor_relay.cpp')).read_text()
 source=source.replace('class RelayLoop {','class RelayLoop {',1).replace('private:', 'public:')
 source=source.replace('int main(int argc, char** argv)', 'int relay_program_main(int argc, char** argv)')
 # Older code lacks the explicit infrastructure abort, only used by the positive cleanup case.
 test=TEST
 if before:test=test.replace('shard.abort_unstarted_match(ch,"test")','shard.close_conn(a,"test")')
 src=OUT/('before.cpp'if before else'after.cpp');src.write_text(source+'\n'+test)
 exe=src.with_suffix('')
 deps=['src/sim_game.cpp','src/position.cpp','server/room_code.cpp','server/log.cpp','net/socket.cpp','net/framing.cpp','net/reactor_epoll.cpp','net/reactor_iocp.cpp','meta/http_client.cpp']
 run(['c++','-std=c++17','-pthread','-g','-O1','-fsanitize=address,undefined','-fno-sanitize-recover=all','-I'+str(R),'-I'+str(R/'server'),str(src),*[str(R/'build-polish/CMakeFiles/tetris_relay_reactor.dir'/(f+'.o'))for f in deps],'-lssl','-lcrypto','-o',str(exe)],timeout=240)
 for case in ['state','full','closed','pending-shutdown','live-shutdown','add-first','add-second','remove-first','remove-second','poll-error']:
  p=subprocess.run([str(exe),case],capture_output=True,text=True,timeout=15)
  (OUT/(exe.name+'-'+case+'.log')).write_text(p.stdout+p.stderr)
  failure=before and case!='state'
  assert(p.returncode!=0 if failure else p.returncode==0),(case,p.stdout,p.stderr)
  print(('before exposed'if failure else'passed'),case,flush=True)
if __name__=='__main__':main()
