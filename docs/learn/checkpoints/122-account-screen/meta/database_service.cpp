#include "meta/sqlite_results.h"
#include "meta/settlement_wire.h"
#include "httplib.h"
#include <charconv>
#include <cstdio>
#include <cstring>
int run(int argc,char** argv) {
    // Numeric loopback is fixed: this unauthenticated fixture is not a public API.
    int requested=0;
    if(argc!=3)return 2;
    const char* end=argv[1]+std::strlen(argv[1]);
    const auto parsed=std::from_chars(argv[1],end,requested);
    if(parsed.ec!=std::errc{} || parsed.ptr!=end || requested<0 || requested>65535)return 2;
    study_meta::SqliteResults store(argv[2]);
    store.seed_demo();
    httplib::Server http;
    http.new_task_queue=[] {return new httplib::ThreadPool(2,8);};
    http.set_payload_max_length(1024);
    http.set_read_timeout(2,0);http.set_write_timeout(2,0);
    http.Get("/healthz",[](const httplib::Request&,httplib::Response& res){
        res.set_content("{\"ok\":true}","application/json");
    });
    auto submit=[&store](const httplib::Request& req,httplib::Response& res,bool version2){
        const auto record=study_meta::parse_record(req.body);
        if(!record){res.status=400;res.set_content("{\"error\":\"invalid\"}","application/json");return;}
        const auto result=store.put(*record);
        using Status=study_meta::SqlStatus;
        if(result.status==Status::accepted) {
            const study_meta::Settlement settlement{result.receipt,
                static_cast<std::uint32_t>(result.awards.a),static_cast<std::uint32_t>(result.awards.b),
                static_cast<std::uint32_t>(result.awards.policy)};
            const auto body=version2 ? study_meta::settlement_json(settlement)
                                     : study_meta::receipt_json(result.receipt);
            res.set_content(body.dump(),"application/json");
        }else {
            res.status=result.status==Status::conflict ? 409 : result.status==Status::storage_error ? 503 : 400;
            res.set_content("{\"error\":\"not_accepted\"}","application/json");
        }
    };
    http.Post("/study/v1/matches",[submit](const httplib::Request& req,httplib::Response& res){submit(req,res,false);});
    http.Post("/study/v2/matches",[submit](const httplib::Request& req,httplib::Response& res){submit(req,res,true);});
    const int port=requested==0 ? http.bind_to_any_port("127.0.0.1")
        : http.bind_to_port("127.0.0.1",requested) ? requested : -1;
    if(port<=0){std::fprintf(stderr,"bind failed\n");return 1;}
    std::printf("PORT %d\n",port);std::fflush(stdout);
    // Store is constructed before http; handlers borrow it only while serving.
    return http.listen_after_bind() ? 0 : 1;
}

int main(int argc,char** argv) {
    try {return run(argc,argv);}
    catch(const std::exception& e){std::fprintf(stderr,"database service: %s\n",e.what());return 1;}
}
