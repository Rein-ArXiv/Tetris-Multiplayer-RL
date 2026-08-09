#pragma once

#include <cstdint>
#include <memory>
#include <mutex>
#include <unordered_set>

namespace relay {

// A ranked player_id may own only one live lease.
class PlayerSessionLease {
public:
    static std::shared_ptr<PlayerSessionLease> acquire(int64_t player_id)
    {
        if (player_id <= 0) return {};
        std::lock_guard<std::mutex> lk(mu_);
        if (!active_.insert(player_id).second) return {};
        return std::shared_ptr<PlayerSessionLease>(new PlayerSessionLease(player_id));
    }

    ~PlayerSessionLease()
    {
        std::lock_guard<std::mutex> lk(mu_);
        active_.erase(player_id_);
    }

    PlayerSessionLease(const PlayerSessionLease&) = delete;
    PlayerSessionLease& operator=(const PlayerSessionLease&) = delete;

private:
    explicit PlayerSessionLease(int64_t player_id) : player_id_(player_id) {}

    int64_t player_id_;
    inline static std::mutex mu_;
    inline static std::unordered_set<int64_t> active_;
};

}  // namespace relay
