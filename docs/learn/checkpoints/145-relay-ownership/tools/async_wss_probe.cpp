// One loopback experiment. Arguments are experiment conditions, not service policy.
#include "net/async_wss_attempt.h"
#include <charconv>
#include <iostream>
#include <map>
using namespace study_async;
int main(int argc,char** argv) {
    if(argc!=6){std::cerr<<"async_wss_probe HOST PORT CA.pem none|immediate|resolve|connect|tls|upgrade|write|read DEADLINE_MS\n";return 2;}
    const std::string mode=argv[4],text=argv[5];int milliseconds=0;
    auto parsed=std::from_chars(text.data(),text.data()+text.size(),milliseconds);
    if(parsed.ec!=std::errc{}||parsed.ptr!=text.data()+text.size()||milliseconds<=0)return 2;
    const std::map<std::string,Stage> stages{{"resolve",Stage::resolve},{"connect",Stage::connect},{"tls",Stage::tls},{"upgrade",Stage::upgrade},{"write",Stage::write},{"read",Stage::read}};
    if(mode!="none"&&mode!="immediate"&&!stages.count(mode))return 2;
    try {
        boost::asio::io_context io;
        boost::asio::ssl::context tls(boost::asio::ssl::context::tls_client);
        tls.load_verify_file(argv[3]);
        Options options{};options.host=argv[1];options.port=argv[2];
        options.payload={3,1,4,1,5,9}; // Deterministic diagnostic bytes, no credentials.
        options.timeout=std::chrono::milliseconds(milliseconds);
        options.message_limit=options.payload.size()*2;
        if(stages.count(mode))options.cancel_at=stages.at(mode);
        auto trace=std::make_shared<Trace>();
        auto attempt=std::make_shared<Attempt>(io,tls,std::move(options),trace);
        std::weak_ptr<Attempt> weak=attempt;
        // Unrelated work on the same executor must still run after this attempt ends.
        bool unrelated=false;boost::asio::steady_timer marker(io);
        marker.expires_after(std::chrono::milliseconds(80));
        marker.async_wait([&](boost::system::error_code error){if(!error)unrelated=true;});
        attempt->begin();
        if(mode=="immediate"){attempt->request_cancel();attempt->request_cancel();}
        attempt.reset();
        if(weak.expired())return 6;
        io.run();
        if(!trace->result||trace->reports!=1||trace->destroyed!=1||!weak.expired()||!unrelated)return 7;
        std::cout<<"{\"end\":"<<static_cast<int>(*trace->result)
                 <<",\"stage\":"<<static_cast<int>(trace->stage)
                 <<",\"reports\":"<<trace->reports<<",\"callbacks\":"<<trace->callbacks
                 <<",\"late\":"<<trace->late_callbacks<<",\"destroyed\":"<<trace->destroyed
                 <<",\"expired\":true,\"unrelated\":true}\n";
        return 0;
    }catch(const std::exception& e){std::cerr<<"diagnostic failure: "<<e.what()<<'\n';return 3;}
}
