#pragma once
#include "../src/sim_game.h"
#include "placement.h"
#include <algorithm>
#include <deque>

namespace bot {
// Frame input scheduling, independent of rendering and model inference speed.
// A changed piece RNG state detects every spawn, including gravity locking while
// a slow bot is still executing an old plan (even if the new piece has the same ID).
class Controller {
public:
    void reset(int interval = 6, int think = 18, int minimum = 60) {
        interval_ = std::clamp(interval, 1, 30);
        think_ = std::clamp(think, 0, 180);
        minimum_ = std::clamp(minimum, 1, 600);
        spawned_ = false; queue_.clear(); cooldown_ = age_ = 0;
    }
    template<class Picker>
    uint8_t next(const SimGame& sim, Picker pick) {
        if (sim.IsGameOver()) return INPUT_NONE;
        if (!spawned_ || pieceRng_ != sim.RngState()) {
            spawned_ = true; pieceRng_ = sim.RngState();
            queue_.clear(); cooldown_ = age_ = 0;
        }
        const int age = age_++;
        if (age < think_) return INPUT_NONE;
        if (cooldown_ > 0) { --cooldown_; return INPUT_NONE; }
        if (queue_.empty()) {
            int col, rot;
            if (!pick(sim, col, rot)) { cooldown_ = interval_ - 1; return INPUT_NONE; }
            const auto plan=expand_placement(sim.CurrentCol(),sim.CurrentRotation(),col,rot);
            queue_.assign(plan.begin(),plan.end());
        }
        if (queue_.empty()) return INPUT_NONE;
        if ((queue_.front() & INPUT_DROP) && age + 1 < minimum_) return INPUT_NONE;
        const auto input=queue_.front(); queue_.pop_front();
        cooldown_=interval_ - 1;
        return input;
    }
private:
    int interval_=6, think_=18, minimum_=60, cooldown_=0, age_=0;
    bool spawned_=false;
    uint64_t pieceRng_=0;
    std::deque<uint8_t> queue_;
};
}
