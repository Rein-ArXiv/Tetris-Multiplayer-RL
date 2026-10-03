#pragma once
#include "meta/ranking_wire.h"
#include "httplib.h"
namespace study_meta {
class RankingTask final {
public:
    explicit RankingTask(int port):port_(port){}
    std::optional<Ranking> operator()()const {
        if(port_<1 || port_>65535)return {};
        try {
            httplib::Client http("127.0.0.1",port_);
            http.set_connection_timeout(2,0);http.set_read_timeout(2,0);http.set_write_timeout(2,0);
            std::string body;
            const auto response=http.Get("/study/v1/ranking",[&](const char* bytes,std::size_t count) {
                if(count>kJsonBodyBytes-body.size())return false;
                body.append(bytes,count);return true;
            });
            if(!response || response->status!=200)return {};
            return parse_ranking(body);
        }catch(const std::exception&) {return {};}
    }
private:
    int port_;
};
} // namespace study_meta
