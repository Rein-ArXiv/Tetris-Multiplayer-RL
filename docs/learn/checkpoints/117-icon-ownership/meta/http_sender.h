#pragma once
#include "meta/wire.h"
#include "httplib.h"
namespace study_meta {
class HttpSender {
public:
    explicit HttpSender(int port) : port_(port) {}
    // One HTTP attempt. MatchSubmission owns any retry count and immutable body.
    study_net::Reply operator()(const study_net::MatchRecord& record) const noexcept {
        try {
            if(port_<1 || port_>65535)return {};
            httplib::Client client("127.0.0.1",port_);
            client.set_connection_timeout(2,0);
            client.set_read_timeout(2,0);
            client.set_write_timeout(2,0);
            const auto response=client.Post("/study/v1/matches",record_json(record).dump(),"application/json");
            if(!response || response->status!=200)return {};
            const auto receipt=parse_receipt(response->body);
            if(!receipt)return {};
            return {study_net::Reply::confirmed,*receipt};
        }catch(const std::exception&) { return {}; }
    }
private:
    int port_;
};
} // namespace study_meta
