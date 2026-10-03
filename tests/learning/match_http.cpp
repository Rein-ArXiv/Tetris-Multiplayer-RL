#include "meta/http_client.h"
#include "httplib.h"
#include <atomic>
#include <cstdio>
#include <stdexcept>
#include <thread>
void check(bool ok) { if(!ok)throw std::runtime_error("match HTTP contract"); }
int main(){
    httplib::Server server;
    std::atomic<unsigned> calls{0},writes{0};std::atomic<bool> same{true};
    std::string body;
    const std::string receipt=R"({ "match_id": 7, "a": {"elo_before":10,"elo_after":15,"delta":5},
        "b": {"elo_before":4,"elo_after":0,"delta":-4} })";
    server.Post("/v1/matches",[&](const httplib::Request& req,httplib::Response& res){
        const auto call=++calls;
        if(call==1){body=req.body;++writes;res.status=503;res.set_content("{}","application/json");return;}
        if(req.body!=body)same=false;
        if(call==3){res.set_content(R"({"match_id":7,"wrapper":{"a":{"elo_before":10,"elo_after":15,"delta":5},"b":{"elo_before":4,"elo_after":0,"delta":-4}}})","application/json");return;}
        res.set_content(receipt,"application/json");
    });
    const auto port=server.bind_to_any_port("127.0.0.1");if(port<=0)return 1;
    std::thread thread([&]{server.listen_after_bind();});
    struct Stop { httplib::Server& server;std::thread& thread;~Stop(){server.stop();thread.join();} } stop{server,thread};
    try {
        server.wait_until_ready();
        meta::client::MetaClient client("http://127.0.0.1:"+std::to_string(port),"fixture-secret");
        check(client.valid());
        const auto send=[&]{return client.post_match("00000000000000000000000000000017",101,202,101,10,4,1,0,1,3);};
        const auto first=send();check(first && first->match_id==7 && first->a.delta==5);
        check(calls==2 && writes==1 && same);
        check(!send());check(calls==3);
        std::puts("HTTP: identical retry after service 503, pretty receipt accepted, wrong-parent receipt rejected");
    }catch(const std::exception& e){std::fprintf(stderr,"%s\n",e.what());return 1;}
}
