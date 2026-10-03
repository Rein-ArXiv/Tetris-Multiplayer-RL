#include "meta/http_client.h"
#include "meta/json_input.h"
#include "httplib.h"
#include <atomic>
#include <thread>
#include <iostream>
#include <vector>
#include <mutex>
using Json=nlohmann::json;
int main() {
    httplib::Server server;std::string response;std::mutex response_mutex;
    auto send=[&](const auto&,auto& res){std::lock_guard<std::mutex> lock(response_mutex);res.set_content(response,"application/json");};
    server.Post("/v1/guest",send);server.Post("/v1/auth/verify",send);
    const int port=server.bind_to_any_port("127.0.0.1");if(port<=0)return 2;
    std::thread t([&]{server.listen_after_bind();});
    struct Stop{httplib::Server& s;std::thread& t;~Stop(){s.stop();t.join();}} stop{server,t};
    server.wait_until_ready();meta::client::MetaClient api("http://127.0.0.1:"+std::to_string(port));
    Json valid={{"player_id",1},{"token",std::string(32,'a')},{"username",nullptr},
        {"elo",0},{"bp",0},{"xp",0},{"selected_icon_id","default"}};
    bool ok=true;
    auto check=[&](const Json& j,bool want,const std::string& label,bool guest_only=false) {
        {std::lock_guard<std::mutex> lock(response_mutex);response=j.dump();}
        auto guest=api.request_guest(1);
        if(bool(guest)!=want){std::cout<<"guest "<<label<<" accepted="<<bool(guest)<<'\n';ok=false;}
        if(want && guest && (guest->player_id!=j["player_id"].get<int64_t>() ||
            guest->bp!=j["bp"].get<int>() || guest->xp!=j.value("xp",0) || guest->elo!=j["elo"].get<int>()))ok=false;
        if(!guest_only) {
            meta::client::MetaClient::VerifyOutcome outcome;
            auto auth=api.verify_token(std::string(32,'a'),1,&outcome);
            if(want && auth && (auth->player_id!=j["player_id"].get<int64_t>() ||
                auth->bp!=j["bp"].get<int>() || auth->xp!=j.value("xp",0) || auth->elo!=j["elo"].get<int>()))ok=false;
            if(bool(auth)!=want || (!want && outcome!=meta::client::MetaClient::VerifyOutcome::NetworkError)){
                std::cout<<"auth "<<label<<" accepted="<<bool(auth)<<'\n';ok=false;
            }
        }
    };
    check(valid,true,"normal");
    auto old=valid;old.erase("xp");check(old,true,"missing-xp-legacy");
    auto extreme=valid;extreme["player_id"]=INT64_MAX;
    for(auto k:{"elo","xp","bp"})extreme[k]=INT32_MAX;
    check(extreme,true,"maxima");
    for(auto k:{"elo","xp","bp"}) {
        for(const Json& v:{Json(-1),Json(2147483648LL),Json(true),Json("0"),Json(nullptr),Json(1.0)}) {
            auto bad=valid;bad[k]=v;check(bad,false,std::string(k)+"-type/range");
        }
    }
    for(const Json& v:{Json(0),Json(-1),Json(UINT64_MAX),Json(true),Json("1")}) {
        auto bad=valid;bad["player_id"]=v;check(bad,false,"id-type/range");
    }
    for(const Json& v:{Json(""),Json(nullptr),Json(0)}) {
        auto bad=valid;bad["selected_icon_id"]=v;check(bad,false,"icon-type/empty");
    }
    for(const Json& v:{Json(1),Json(true),Json::object()}) {
        auto bad=valid;bad["username"]=v;check(bad,false,"username-type");
    }
    for(auto token:{std::string("chosen"),std::string(32,'A'),std::string(32,'a')+'\0'}) {
        auto bad=valid;bad["token"]=token;check(bad,false,"token-format",true);
    }
    auto ext=valid;ext["future"]="kept-compatible";check(ext,true,"extension");
    if(ok)std::cout<<"Actual account responses: bounds/types, missing-only legacy XP, credential shape and extension compatibility passed\n";
    return ok?0:1;
}
