#pragma once
#include "meta/issued_wire.h"
#include "meta/profile_wire.h"
#include "httplib.h"
namespace study_meta {
enum class VerifyState { accepted, rejected, unavailable };
struct VerifiedProfile {
    VerifyState state = VerifyState::unavailable;
    std::optional<PublicProfile> profile;
};
class AccountHttp {
public:
    explicit AccountHttp(int port) : port_(port) {}
    bool valid() const { return port_ > 0 && port_ <= 65535; }
    std::string origin() const { return valid() ? "http://127.0.0.1:" + std::to_string(port_) : ""; }
    std::optional<IssuedAccount> issue() const {
        if (!valid()) return std::nullopt;
        try {
            httplib::Client client("127.0.0.1",port_); limits(client);
            const auto reply = client.Post("/study/v1/guest","{}","application/json");
            if (!reply || reply->status != 201) return std::nullopt;
            return parse_issued(reply->body);
        } catch (const std::exception&) { return std::nullopt; }
    }
    VerifiedProfile verify(const std::string& token) const {
        if (!valid() || !credential_hex(token,32)) return {};
        try {
            httplib::Client client("127.0.0.1",port_); limits(client);
            const auto reply = client.Get("/study/v1/me",{{"Authorization","Bearer " + token}});
            if (!reply) return {};
            if (reply->status == 401) return {VerifyState::rejected, {}};
            if (reply->status != 200) return {};
            const auto profile = parse_profile(reply->body);
            return profile ? VerifiedProfile{VerifyState::accepted, profile} : VerifiedProfile{};
        } catch (const std::exception&) { return {}; }
    }
private:
    static void limits(httplib::Client& client) {
        client.set_connection_timeout(2,0);client.set_read_timeout(2,0);client.set_write_timeout(2,0);
    }
    int port_;
};
} // namespace study_meta
