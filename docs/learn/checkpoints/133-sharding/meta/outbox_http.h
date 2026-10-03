#pragma once
#include "meta/http_sender.h"
namespace study_meta {
// The origin and transport are derived from the same endpoint value.
class OutboxHttp {
public:
    explicit OutboxHttp(int port) : port_(port) {}
    bool valid() const { return port_ > 0 && port_ <= 65535; }
    std::string origin() const { return valid() ? "http://127.0.0.1:" + std::to_string(port_) : ""; }
    study_net::Reply operator()(const study_net::MatchRecord& record,
                               std::chrono::milliseconds limit) const noexcept {
        return HttpSender(port_)(record, limit);
    }
private:
    int port_;
};
} // namespace study_meta
