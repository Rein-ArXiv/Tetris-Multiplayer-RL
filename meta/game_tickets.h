#pragma once
#include <chrono>
#include <cstddef>
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
    static constexpr std::size_t max_pending = 4096;
    struct Admission { int64_t player; int64_t epoch; };
    bool issue(const std::string& ticket, int64_t player, int64_t epoch = 0,
               Clock::time_point now = Clock::now()) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (now > Clock::time_point::max() - lifetime) return false;
        bool replacing = false;
        for (auto it = entries_.begin(); it != entries_.end();) {
            if (it->second.expires <= now) it = entries_.erase(it);
            else {
                replacing = replacing || it->second.player == player;
                ++it;
            }
        }
        // A failed replacement must leave the player's live ticket usable.
        // Keep other players' tickets even when the random candidate collides.
        if (entries_.find(ticket) != entries_.end()) return false;
        if (!replacing && entries_.size() >= max_pending) return false;
        const auto inserted = entries_.emplace(ticket, Entry{player, epoch, now + lifetime});
        if (!inserted.second) return false;
        // Insert may allocate/rehash. Remove old entries only after it succeeds;
        // no external observer sees the temporary extra entry under this mutex.
        for (auto it = entries_.begin(); it != entries_.end();) {
            if (it != inserted.first && it->second.player == player) it = entries_.erase(it);
            else ++it;
        }
        return true;
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
