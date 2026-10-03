#include "meta/http_client.h"
#include "httplib.h"
#include <atomic>
#include <iostream>
#include <thread>
#include <stdexcept>
void check(bool ok){if(!ok)throw std::runtime_error("HTTP failure contract");}
int main() {
    httplib::Server server;
    std::atomic<int> status{400}, calls{0};
    std::atomic<bool> malformed{false};
    const std::string marker="FAKE-CREDENTIAL-119\n[forged log line]";
    const std::string receipt=R"({"match_id":1,"a":{"elo_before":0,"elo_after":0,"delta":0},"b":{"elo_before":0,"elo_after":0,"delta":0}})";
    server.Post("/v1/guest",[&](const auto&,auto& res){res.status=400;res.set_content(marker,"text/plain");});
    server.Post("/v1/auth/verify",[&](const auto& req,auto& res){res.status=500;res.set_content(req.body+"\n"+marker,"text/plain");});
    server.Post("/v1/matches",[&](const auto&,auto& res){
        ++calls;res.status=status.load();
        res.set_content(res.status==200 ? (malformed ? "{}" : receipt) : marker,"application/json");
    });
    const int port=server.bind_to_any_port("127.0.0.1");if(port<=0)return 1;
    std::thread thread([&]{server.listen_after_bind();});
    struct Stop{httplib::Server& s;std::thread& t;~Stop(){s.stop();t.join();}} stop{server,thread};
    try {
        server.wait_until_ready();
        meta::client::MetaClient client("http://127.0.0.1:"+std::to_string(port));
        check(!client.request_guest(1));check(!client.verify_token(marker,1));
        auto send=[&]{return client.post_match("00000000000000000000000000000119",1,2,1,1,0,0,0,1,5);};
        for(int code:{400,401,403,409,429,500,503}) {
            calls=0;status=code;check(!send());check(calls==(code==429 || code>=500 ? 3 : 1));
        }
        calls=0;status=200;malformed=true;check(!send());check(calls==1);
        calls=0;malformed=false;check(send().has_value());check(calls==1);
        meta::client::MetaClient bad("http://FAKE-CREDENTIAL-119@example.invalid");
        check(!bad.valid());
        std::cout<<"Actual HTTP: status retry counts, malformed success, normal receipt and diagnostic fixtures passed\n";
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
