#pragma once
#include "meta/wire.h"
#include "meta/http_retry_policy.h"
#include "httplib.h"
#include <chrono>
namespace study_meta {
class HttpSender {
public:
    explicit HttpSender(int port) : port_(port) {}
    // One attempt. The driver owns waiting/counts; each I/O phase gets a limit.
    study_net::Reply operator()(const study_net::MatchRecord& record,
                                std::chrono::milliseconds phase_limit) const noexcept {
        using Reply = study_net::Reply;
        try {
            if (port_ < 1 || port_ > 65535 || phase_limit.count() <= 0)
                return {Reply::stopped, {}};
            httplib::Client client("127.0.0.1", port_);
            client.set_connection_timeout(phase_limit);
            client.set_read_timeout(phase_limit);
            client.set_write_timeout(phase_limit);
            const auto response = client.Post("/study/v1/matches",
                record_json(record).dump(), "application/json");
            if (!response) return {};
            switch (http_action(response->status)) {
            case HttpAction::stop: return {Reply::stopped, {}};
            case HttpAction::retry: {
                std::chrono::milliseconds delay{0};
                const auto count = response->get_header_value_count("Retry-After");
                if (count > 1) return {Reply::stopped, {}};
                if (count == 1) {
                    const auto hint = retry_after_seconds(response->get_header_value("Retry-After"));
                    if (!hint) return {Reply::stopped, {}};
                    delay = *hint;
                }
                return {Reply::unconfirmed, {}, delay};
            }
            case HttpAction::inspect_receipt:
                const auto receipt = parse_receipt(response->body);
                if (!receipt) return {Reply::stopped, {}};
                return {Reply::confirmed, *receipt};
            }
        } catch (const std::exception&) {
            // Local errors stop automatic work; never log a remote body here.
        }
        return {Reply::stopped, {}};
    }
    study_net::Reply operator()(const study_net::MatchRecord& record) const noexcept {
        return (*this)(record, std::chrono::milliseconds{2000});
    }
private:
    int port_;
};
} // namespace study_meta
