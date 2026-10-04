#pragma once
#include "meta/journal_wire.h"
#include "meta/account_http.h"
namespace study_meta {
class JournalHttp {
public:
    explicit JournalHttp(int port):port_(port){}
    bool valid() const { return port_>0 && port_<=65535; }
    std::string origin() const { return AccountHttp(port_).origin(); }
    RemoteChange change(const PendingChange& p) const {
        if (!valid() || p.origin!=origin() || !valid_pending(p)) return {};
        try {
            httplib::Client client("127.0.0.1",port_);
            client.set_connection_timeout(2,0);client.set_read_timeout(2,0);client.set_write_timeout(2,0);
            const auto body=Json{{"credential",p.credential},{"next_token",p.next_token},
                {"next_recovery",p.next_recovery}}.dump();
            const auto reply=client.Post(("/study/v1/account/"+p.operation).c_str(),body,"application/json");
            if (!reply) return {};
            if (reply->status==400 || reply->status==401 || reply->status==409)
                return {RemoteState::rejected,0,0};
            if (reply->status!=200) return {};
            const auto j=object(reply->body,2);
            if (!j) return {};
            const auto id=number(*j,"player_id"),epoch=number(*j,"auth_epoch");
            if (!id || !*id || !epoch || *epoch>static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) return {};
            return {RemoteState::accepted,*id,*epoch};
        } catch (const std::exception&) { return {}; }
    }
private:int port_;
};
} // namespace study_meta
