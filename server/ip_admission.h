// server/ip_admission.h — per-IP 입장 제어 (두 릴레이 바이너리 공용)
//
// Handshake slots cover admission work; session slots remain owned through
// queue/room/game cleanup. Both are simultaneous occupancy caps, not request
// rates. Repeated short connections require separate rate/work limits.
// The final shared owner releases each reservation once. An IP can represent
// many people (NAT), and one actor can use many IPs; this is a resource policy,
// not proof of identity or complete abuse prevention.

#pragma once

#include <cstddef>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>

namespace relay {

// 동시 핸드셰이크 예산. 인증이 끝나면 즉시 반납되므로 짧게 잡아도 된다.
constexpr size_t kMaxHandshakesPerIp = 16;

// Default concurrent sessions per source. This is an operating policy, not a
// measured capacity or fairness guarantee. Tune against real shared-IP demand.
// Worker, queue and connection caps cover different populations; their numeric
// values must not be equated. Either per-IP phase cap may be the tighter one.
constexpr size_t kMaxSessionsPerIp = 64;

class IpAdmission {
public:
    enum class Kind { Handshake, Session };

    // 기동 시 인자 파싱 직후 한 번만 호출한다 (accept 시작 전).
    static void set_session_limit(size_t n)
    {
        std::lock_guard<std::mutex> lk(mu_);
        session_limit_ = (n == 0) ? 1 : n;
    }

    static size_t session_limit()
    {
        std::lock_guard<std::mutex> lk(mu_);
        return session_limit_;
    }

    // 슬롯 하나를 잡는다. 상한에 걸리면 nullptr — 호출자는 연결을 거절한다.
    // key 는 보통 peer IP. getpeername 이 실패했을 때 모든 실패 연결이 하나의
    // 버킷을 공유해 서로를 굶기지 않도록, 호출자가 연결마다 고유한 키를 대신
    // 넘길 수 있다 (per-IP 상한은 못 걸지만 공멸보다는 낫다).
    static std::shared_ptr<IpAdmission> acquire(std::string key, Kind kind)
    {
        if (key.empty()) key = "unknown";
        // Finish ownership allocations before publishing a reservation. If the
        // shared_ptr control block fails, its unregistered object does no cleanup.
        auto candidate = std::shared_ptr<IpAdmission>(
            new IpAdmission(std::move(key), kind));
        std::lock_guard<std::mutex> lk(mu_);
        auto& table = (kind == Kind::Handshake) ? handshakes_ : sessions_;
        const size_t limit = (kind == Kind::Handshake) ? kMaxHandshakesPerIp
                                                       : session_limit_;
        auto it = table.find(candidate->key_);
        const size_t n = (it == table.end()) ? 0u : it->second;
        if (n >= limit) return {};
        // try_emplace may allocate. Failure leaves no reservation to roll back.
        if (it == table.end()) it = table.try_emplace(candidate->key_, 0).first;
        ++it->second;
        candidate->registered_ = true; // No throwing work after this commit.
        return candidate;
    }

    ~IpAdmission()
    {
        if (!registered_) return;
        std::lock_guard<std::mutex> lk(mu_);
        auto& table = (kind_ == Kind::Handshake) ? handshakes_ : sessions_;
        auto it = table.find(key_);
        if (it == table.end()) return;
        if (--it->second == 0) table.erase(it);
    }

    IpAdmission(const IpAdmission&) = delete;
    IpAdmission& operator=(const IpAdmission&) = delete;

private:
    IpAdmission(std::string key, Kind kind)
        : key_(std::move(key)), kind_(kind) {}

    std::string key_;
    Kind        kind_;
    bool        registered_ = false;

    inline static std::mutex                             mu_;
    inline static std::unordered_map<std::string, size_t> handshakes_;
    inline static std::unordered_map<std::string, size_t> sessions_;
    inline static size_t                                  session_limit_ =
        kMaxSessionsPerIp;
};

}  // namespace relay
