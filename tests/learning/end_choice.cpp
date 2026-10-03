#include "net/session.h"
#include <cstdio>
#include <cstdlib>
using namespace net;
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"line %d: %s\n",__LINE__,#x);return 1;}}while(false)
#define NET_WARN(x) do{}while(false)
#define NET_TRACE(x) do{}while(false)
struct ProbeSession {
 std::atomic<uint8_t> localGameOverChoice{0},remoteGameOverChoice{0};
 std::atomic_bool connectionFailed{false},quit{false};
 std::mutex matchResultMu_;bool matchResultValid_=true;
 using MatchResult=net::Session::MatchResult;MatchResult matchResult_{};
 std::vector<std::vector<uint8_t>> queued;
 void pushSend(std::vector<uint8_t>&&f){queued.push_back(std::move(f));}
 void SendGameOverChoice(GameOverChoice);
 bool GetRemoteGameOverChoice(GameOverChoice&)const;
 void ClearGameOverChoices();
 void handle(const Frame&f){switch(f.type){
#include "choice_case.inc"
 default:break;
 }}
};
#include "choice_methods.inc"
int main(){
 for(unsigned size=0;size<=4;++size)for(unsigned raw=0;raw<256;++raw){ProbeSession s;Frame f;f.type=MsgType::GAME_OVER_CHOICE;f.payload.assign(size,0);if(size)f.payload[0]=raw;s.handle(f);
  CHECK(s.remoteGameOverChoice==((size==1&&(raw==1||raw==2))?raw:0));CHECK(!s.quit&&!s.connectionFailed);
 }
 for(unsigned raw=0;raw<256;++raw){ProbeSession s;s.SendGameOverChoice(static_cast<GameOverChoice>(raw));bool valid=raw==1||raw==2;CHECK(s.queued.size()==(valid?1u:0u));CHECK(s.localGameOverChoice==(valid?raw:0));}
 ProbeSession s;Frame f;f.type=MsgType::GAME_OVER_CHOICE;f.payload={1};s.handle(f);s.handle(f);CHECK(s.remoteGameOverChoice==1&&!s.quit);
 f.payload={2};s.handle(f);CHECK(s.remoteGameOverChoice==1&&s.quit&&s.connectionFailed);
 GameOverChoice out=GameOverChoice::None;CHECK(s.GetRemoteGameOverChoice(out)&&out==GameOverChoice::Restart);
 s.ClearGameOverChoices();out=GameOverChoice::GoToTitle;CHECK(!s.GetRemoteGameOverChoice(out)&&out==GameOverChoice::GoToTitle);CHECK(s.localGameOverChoice==0&&!s.matchResultValid_);
 std::puts("actual choices: exact byte/domain, send validation, duplicate/conflict, atomic first choice, reset/output preservation");
}
