#include "meta/sqlite_results.h"
#include "meta/ranking_route.h"
#include "meta/account_crypto.h"
#include "httplib.h"
#include "platform/utf8_arguments.h"
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
    store.seed_shop();
    httplib::Server http;
    study_meta::ranking_route(http,store);
    http.new_task_queue=[] {return new httplib::ThreadPool(2,8);};
    http.set_payload_max_length(study_meta::kJsonBodyBytes);
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
    auto change=[&store](const httplib::Request& req,httplib::Response& res,study_meta::ChangeKind kind) {
        const auto body=study_meta::object(req.body,3);
        if(req.get_header_value("Content-Type")!="application/json" || !req.params.empty() || !body ||
           !body->contains("credential") || !body->at("credential").is_string() ||
           !body->contains("next_token") || !body->at("next_token").is_string() ||
           !body->contains("next_recovery") || !body->at("next_recovery").is_string()) {
            reply(res,400,{{"error","invalid_request"}});return;
        }
        try {
            const study_meta::ChangeRequest input{kind,body->at("credential").get<std::string>(),
                body->at("next_token").get<std::string>(),body->at("next_recovery").get<std::string>()};
            const auto result=store.change_account(input);
            using S=study_meta::ChangeStatus;
            switch(result.status) {
            case S::ok:reply(res,200,{{"player_id",result.player_id},{"auth_epoch",result.epoch}});return;
            case S::invalid_request:reply(res,400,{{"error","invalid_request"}});return;
            case S::invalid_credential:reply(res,401,{{"error","invalid_credential"}});return;
            case S::conflict:reply(res,409,{{"error","credential_conflict"}});return;
            case S::storage_error:reply(res,503,{{"error","change_unavailable"}});return;
            }
        } catch(const std::exception&) {reply(res,503,{{"error","change_unavailable"}});}
    };
    http.Post("/study/v1/account/backup",[change](const auto& req,auto& res){change(req,res,study_meta::ChangeKind::backup);});
    http.Post("/study/v1/account/rotate",[change](const auto& req,auto& res){change(req,res,study_meta::ChangeKind::rotate);});
    http.Post("/study/v1/account/recover",[change](const auto& req,auto& res){change(req,res,study_meta::ChangeKind::recover);});
    http.Get("/study/v1/icons/catalog",[](const httplib::Request& req,httplib::Response& res) {
        if(!req.params.empty() || !req.body.empty()){reply(res,400,{{"error","invalid_request"}});return;}
        auto icons=study_meta::Json::array();
        for(const auto& icon:study_meta::kShopIcons)
            icons.push_back({{"id",icon.id},{"label",icon.label},{"price_bp",icon.price}});
        reply(res,200,icons);
    });
    auto shop=[&store](const httplib::Request& req,httplib::Response& res,const std::string& action) {
        if(!req.params.empty()){reply(res,400,{{"error","invalid_request"}});return;}
        const auto header=req.get_header_value("Authorization");
        if(req.get_header_value_count("Authorization")!=1 || header.compare(0,7,"Bearer ")!=0 ||
           !study_meta::credential_hex(header.substr(7),32)) {
            reply(res,401,{{"error","invalid_credential"}});return;
        }
        const auto icon=action=="inventory" ? std::optional<std::string>("") : study_meta::parse_icon_choice(req.body);
        if(!icon || (action=="inventory" ? !req.body.empty() : req.get_header_value("Content-Type")!="application/json")) {
            reply(res,400,{{"error","invalid_request"}});return;
        }
        try {
            const auto hash=study_meta::account_hash(header.substr(7));
            const auto result=action=="inventory" ? store.inventory(hash) :
                action=="buy" ? store.buy_icon(hash,*icon) : store.choose_icon(hash,*icon);
            using S=study_meta::ShopStatus;
            switch(result.status) {
            case S::ok:reply(res,200,study_meta::shop_json(*result.view));return;
            case S::unknown_account:reply(res,401,{{"error","invalid_credential"}});return;
            case S::unknown_icon:reply(res,400,{{"error","unknown_icon"}});return;
            case S::already_owned:reply(res,409,{{"error","already_owned"}});return;
            case S::insufficient_bp:reply(res,402,{{"error","insufficient_bp"}});return;
            case S::not_owned:reply(res,403,{{"error","not_owned"}});return;
            case S::storage_error:reply(res,503,{{"error","shop_unavailable"}});return;
            }
        }catch(const std::exception&) {reply(res,503,{{"error","shop_unavailable"}});}
    };
    http.Get("/study/v1/inventory",[shop](const httplib::Request& req,httplib::Response& res){shop(req,res,"inventory");});
    http.Post("/study/v1/icons/buy",[shop](const httplib::Request& req,httplib::Response& res){shop(req,res,"buy");});
    http.Post("/study/v1/icons/select",[shop](const httplib::Request& req,httplib::Response& res){shop(req,res,"select");});
    // Numeric loopback only. Credential changes are exercised locally; this service is not a public deployment.
    const int port=requested==0 ? http.bind_to_any_port("127.0.0.1")
        : http.bind_to_port("127.0.0.1",requested) ? requested : -1;
    if(port<=0)return 1;
    std::printf("PORT %d\n",port);std::fflush(stdout);
    return http.listen_after_bind() ? 0 : 1;
}
}
int guarded_run(int argc, char** argv) {
    try { return run(argc, argv); }
    catch (const std::exception&) {
        std::fprintf(stderr, "account service unavailable\n");
        return 1;
    }
}
#ifdef _WIN32
int wmain(int argc, wchar_t** argv) {
    try {
        auto strings = platform::utf8_arguments(argc, argv);
        // Construct the borrowed pointers after the owning strings stop moving.
        std::vector<char*> arguments;
        arguments.reserve(strings.size() + 1);
        for (auto& value : strings) arguments.push_back(value.data());
        arguments.push_back(nullptr);
        return guarded_run(argc, arguments.data());
    } catch (const std::exception&) {
        std::fprintf(stderr, "command line initialization failed\n");
        return 1;
    }
}
#else
int main(int argc, char** argv) {
    return guarded_run(argc, argv);
}
#endif

