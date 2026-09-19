#pragma once
#include <chrono>
#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>

namespace meta {
// Short-lived, single-use admission credentials. No persistent account token is
// retained. The mutex covers lookup AND erase, so competing relays cannot redeem
// the same credential twice. Time is injectable for deterministic expiry tests.
class GameTickets {
public:
    using Clock = std::chrono::steady_clock;
    static constexpr auto lifetime = std::chrono::seconds(60);
    struct Admission { int64_t player; int64_t epoch; };
    bool issue(const std::string& ticket, int64_t player, int64_t epoch = 0,
               Clock::time_point now = Clock::now()) {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto it = entries_.begin(); it != entries_.end();) {
            if (it->second.expires <= now || it->second.player == player) it = entries_.erase(it);
            else ++it;
        }
        if (entries_.size() >= 4096) return false;
        return entries_.emplace(ticket, Entry{player, epoch, now + lifetime}).second;
    }
    std::optional<Admission> consume(const std::string& ticket,
                                      Clock::time_point now = Clock::now()) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = entries_.find(ticket);
        if (it == entries_.end()) return std::nullopt;
        auto entry = std::move(it->second);
        entries_.erase(it); // Burn even an expired ticket; never cache successful redemptions.
        if (entry.expires <= now) return std::nullopt;
        return Admission{entry.player,entry.epoch};
    }
private:
    struct Entry { int64_t player; int64_t epoch; Clock::time_point expires; };
    std::mutex mutex_;
    std::unordered_map<std::string, Entry> entries_;
};
}
