"""Compile the actual auth continuation with value/service doubles; no HTTP."""
from pathlib import Path
import subprocess,sys
from check_learning_text_layout import run
R=Path(__file__).resolve().parents[1];OUT=R/'out/learning-checkpoints/131-auth-state'
PRE=r'''
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <unordered_set>
#include <stdexcept>
#include <cstdio>
#define RLOG_INFO(...) ((void)0)
#define RLOG_DEBUG(...) ((void)0)
namespace meta::client {struct AuthInfo{int player_id=1,elo=0;std::string username,selected_icon_id;};}
struct PlayerSessionLease{static std::shared_ptr<PlayerSessionLease>acquire(int){return std::make_shared<PlayerSessionLease>();}};
enum class Stage{Auth,Queued,Dead};
struct Conn{Stage stage=Stage::Auth;int player_id=0,elo=0;std::string username,token,icon;std::shared_ptr<int>auth_cancel=std::make_shared<int>(1);std::shared_ptr<PlayerSessionLease>lease;};
struct Fixture{
 Conn connection;bool present=true;int released=0,applied=0;
 std::unordered_set<uint32_t>pending_auth_{1};
 Conn*find_by_id(uint32_t id){return present&&id==1&&connection.stage!=Stage::Dead?&connection:nullptr;}
 void release_pending_auth_ip(uint32_t){++released;}
 void close_conn(Conn*c,const char*){c->stage=Stage::Dead;}
 void after_auth(Conn*c){++applied;c->stage=Stage::Queued;}
'''
POST=r'''
};
void require(bool ok){if(!ok)throw std::runtime_error("auth state contract");}
int main(){Fixture f;const meta::client::AuthInfo auth;
 f.resume_auth(1,auth,"value");require(f.applied==1&&f.released==1&&f.connection.stage==Stage::Queued);
 f.resume_auth(1,std::nullopt,{});require(f.connection.stage==Stage::Queued&&f.applied==1&&f.released==1);
 Fixture g;g.present=false;g.resume_auth(1,auth,{});require(g.applied==0);
 Fixture h;h.resume_auth(1,std::nullopt,{});require(h.connection.stage==Stage::Dead&&h.applied==0&&h.released==1);
 std::puts("actual auth continuation: duplicate/wrong-stage/removed/failure guarded");}
'''
def main():
 OUT.mkdir(parents=True,exist_ok=True);before='--before'in sys.argv
 p=R/('out/learning-jobs/131-relay-before.cpp'if before else'server/reactor_relay.cpp');s=p.read_text();a=s.index('    void resume_auth(');method=s[a:s.index('    Conn* find_by_id(',a)]
 src=OUT/('before.cpp'if before else'after.cpp');src.write_text(PRE+method+POST);exe=src.with_suffix('')
 run(['c++','-std=c++17','-fsanitize=address,undefined','-fno-sanitize-recover=all',str(src),'-o',str(exe)])
 p=subprocess.run([str(exe)],capture_output=True,text=True,timeout=10);(OUT/(exe.name+'.log')).write_text(p.stdout+p.stderr)
 assert (p.returncode!=0 if before else p.returncode==0),(p.stdout,p.stderr)
 print('before duplicate failure closes an admitted connection'if before else p.stdout)
if __name__=='__main__':main()
