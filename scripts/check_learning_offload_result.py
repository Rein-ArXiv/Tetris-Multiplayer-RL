"""Compile actual post_result/on_result_saved methods with service and socket-effect doubles."""
from pathlib import Path
from check_learning_text_layout import run
ROOT=Path(__file__).resolve().parents[1];OUT=ROOT/'out/learning-checkpoints/130-result-path'
def main():
 OUT.mkdir(parents=True,exist_ok=True);text=(ROOT/'server/reactor_relay.cpp').read_text();a=text.index('    void post_result(');b=text.index('    // 채널이 붙들고 있는 소켓 복사본',a);methods=text[a:b]
 source=r'''
#include "server/offload.h"
#include <optional>
#include <unordered_map>
#include <memory>
#include <string>
#include <cstdio>
#define RLOG_WARN(...) ((void)0)
#define RLOG_INFO(...) ((void)0)
namespace net { enum class ResultStatus { Unknown, SaveFailed }; }
namespace meta { namespace client {
struct Delta { int elo_before=0,elo_after=0,delta=0; };
struct MatchResult { Delta a,b; };
struct MetaClient {
    bool fail=false; int calls=0;
    std::optional<MatchResult> post_match(const std::string&,int64_t,int64_t,
        std::optional<int64_t>,int,int,int,int,int) {
        ++calls;
        if(fail)throw std::runtime_error("service throw");
        return MatchResult{};
    }
};
}}
struct Channel {
    uint32_t match_id=7; std::string match_uuid="test-match";
    int64_t a_id=1,b_id=2; int a_elo=10,b_elo=20;
    bool finalize_inflight=false,close_survivor_pending=true;
    net::ResultStatus result_status=net::ResultStatus::Unknown;
};
using Offload=relay::Offload;
struct Fixture {
    meta::client::MetaClient service;
    meta::client::MetaClient* meta_=&service;
    std::unique_ptr<Offload> offload_=std::make_unique<Offload>(1, []{}, 1);
    std::unordered_map<uint32_t,std::unique_ptr<Channel>> channels_;
    int sent=0,closed=0;
    Fixture(){channels_[7]=std::make_unique<Channel>();}
    void send_result_frames(Channel*,int,int,int,int,int,int){++sent;}
    void close_channel_survivor(Channel*,const char*){++closed;}
METHODS
    void finish(){offload_->shutdown();std::vector<Offload::Cont> out;offload_->drain(out);for(auto& c:out)c();}
};
void require(bool ok){if(!ok)throw std::runtime_error("result path contract");}
void failed(Fixture& f){auto& c=*f.channels_.at(7);require(!c.finalize_inflight&&!c.close_survivor_pending&&c.result_status==net::ResultStatus::SaveFailed&&f.sent==1&&f.closed==1);}
int main(){
 try{
    {Fixture f;require(f.offload_->submit([] {return []{};}));
     f.post_result(f.channels_[7].get(),{},0,0,0,0,0);failed(f);f.finish();require(f.service.calls==0);}
    {Fixture f;f.service.fail=true;f.post_result(f.channels_[7].get(),{},0,0,0,0,0);
     f.finish();failed(f);require(f.service.calls==1);}
    {Fixture f;f.post_result(f.channels_[7].get(),{},0,0,0,0,0);
     f.offload_->shutdown();f.channels_.erase(7);f.finish();require(f.sent==0&&f.closed==0);}
    std::puts("actual result methods: capacity rejection/worker exception finalize and close; late result discarded");
 }catch(const std::exception& e){std::fprintf(stderr,"%s\n",e.what());return 1;}
}
'''.replace('METHODS',methods)
 src=OUT/'result.cpp';src.write_text(source);exe=OUT/'result'
 run(['c++','-std=c++17','-pthread','-fsanitize=address,undefined','-fno-sanitize-recover=all','-I'+str(ROOT),str(src),'-o',str(exe)])
 print(run([str(exe)],timeout=15).stdout,flush=True)
if __name__=='__main__':main()
