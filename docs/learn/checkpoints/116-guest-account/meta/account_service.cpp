#include "meta/sqlite_results.h"
#include "meta/account_crypto.h"
#include "httplib.h"
#include <charconv>
#include <cstdio>
#include <cstring>
namespace {
void reply(httplib::Response& res,int status,const study_meta::Json& body) {
    res.status=status;
    res.set_header("Cache-Control","no-store");
    res.set_content(body.dump(),"application/json");
}
int run(int argc,char** argv) {
    if(argc!=3)return 2;
    int requested=0;const char* end=argv[1]+std::strlen(argv[1]);
    const auto parsed=std::from_chars(argv[1],end,requested);
    if(parsed.ec!=std::errc{} || parsed.ptr!=end || requested<0 || requested>65535)return 2;
    study_meta::SqliteResults store(argv[2]);
    httplib::Server http;
    http.new_task_queue=[] {return new httplib::ThreadPool(2,8);};
    http.set_payload_max_length(1024);
    http.set_read_timeout(2,0);http.set_write_timeout(2,0);
    http.Get("/healthz",[](const httplib::Request&,httplib::Response& res){reply(res,200,{{"ok",true}});});
    http.Post("/study/v1/guest",[&store](const httplib::Request& req,httplib::Response& res) {
        if(req.get_header_value("Content-Type")!="application/json" || !req.params.empty() ||
           !study_meta::object(req.body,0)) {
            reply(res,400,{{"error","invalid_request"}});return;
        }
        try {
            for(int attempt=0;attempt<4;++attempt) {
                const auto candidate=study_meta::new_identity();
                if(candidate.player_id==0)continue;
                const auto result=store.register_account(candidate.player_id,study_meta::account_hash(candidate.token));
                if(result==study_meta::SqliteResults::Registration::collision)continue;
                reply(res,201,{{"player_id",candidate.player_id},{"token",candidate.token}});return;
            }
        }catch(const std::exception&) {}
        reply(res,503,{{"error","registration_unavailable"}});
    });
    http.Get("/study/v1/me",[&store](const httplib::Request& req,httplib::Response& res) {
        if(!req.params.empty() || !req.body.empty()) {reply(res,400,{{"error","invalid_request"}});return;}
        const auto header=req.get_header_value("Authorization");
        if(req.get_header_value_count("Authorization")!=1 || header.compare(0,7,"Bearer ")!=0 ||
           !study_meta::credential_hex(header.substr(7),32)) {
            reply(res,401,{{"error","invalid_credential"}});return;
        }
        try {
            const auto profile=store.profile_by_hash(study_meta::account_hash(header.substr(7)));
            if(!profile){reply(res,401,{{"error","invalid_credential"}});return;}
            reply(res,200,study_meta::profile_json(*profile));
        }catch(const std::exception&) {reply(res,503,{{"error","profile_unavailable"}});}
    });
    // Numeric loopback only. This fixture has no public deployment or recovery flow.
    const int port=requested==0 ? http.bind_to_any_port("127.0.0.1")
        : http.bind_to_port("127.0.0.1",requested) ? requested : -1;
    if(port<=0)return 1;
    std::printf("PORT %d\n",port);std::fflush(stdout);
    return http.listen_after_bind() ? 0 : 1;
}
}
int main(int argc,char** argv) {
    try{return run(argc,argv);}
    catch(const std::exception&){std::fprintf(stderr,"account service unavailable\n");return 1;}
}
