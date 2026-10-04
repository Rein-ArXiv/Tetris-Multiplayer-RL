#pragma once
#include "../src/sim_game.h"
#include "placement.h"
#include "pacing.h"
#include <deque>

namespace bot {
// Frame input scheduling, independent of rendering and model inference speed.
// A changed piece RNG state detects every spawn, including gravity locking while
// a slow bot is still executing an old plan (even if the new piece has the same ID).
class Controller {
public:
    enum class Status { waiting, input, no_decision, invalid_target, blocked, target_lost, finished };
    Status status() const noexcept { return status_; }
    void reset(int interval = Pacing{}.interval, int think = Pacing{}.think, int minimum = Pacing{}.minimum) {
        gate_.reset(Pacing::clamped(interval,think,minimum));
        spawned_ = false; queue_.clear();
        status_ = Status::waiting;
    }
    template<class Picker>
    uint8_t next(const SimGame& sim, Picker pick) {
        status_ = Status::waiting;
        if (sim.IsGameOver()) { queue_.clear(); status_=Status::finished; return INPUT_NONE; }
        if (!spawned_ || pieceRng_ != sim.RngState()) {
            spawned_ = true; pieceRng_ = sim.RngState();
            queue_.clear(); gate_.new_piece();
        }
        if (!gate_.begin_tick()) return INPUT_NONE;
        if (queue_.empty()) {
            int col=-1, rot=-1;
            if (!pick(sim, col, rot)) {
                gate_.defer(); status_=Status::no_decision; return INPUT_NONE;
            }
            if(col<0 || col>=kNumCols || rot<0 || rot>=kNumRotations) {
                gate_.defer(); status_=Status::invalid_target; return INPUT_NONE;
            }
            const auto plan=expand_placement(sim.CurrentCol(),sim.CurrentRotation(),col,rot);
            queue_.assign(plan.begin(),plan.end());
            targetCol_=col; targetRot_=rot;
        }
        if (queue_.empty()) return INPUT_NONE;
        if ((queue_.front() & INPUT_DROP) && !gate_.drop_ready()) return INPUT_NONE;
        const auto input=queue_.front();
        if(input==INPUT_DROP) {
            if(sim.CurrentCol()!=targetCol_ || sim.CurrentRotation()!=targetRot_) {
                queue_.clear(); gate_.defer(); status_=Status::target_lost; return INPUT_NONE;
            }
        } else {
            // Check the immediate command on a value copy. Gravity is still
            // consumed by the caller's real Tick; it is not simulated twice.
            SimGame probe=sim;
            probe.SubmitInput(input);
            const int expectedCol=sim.CurrentCol()+(input==INPUT_RIGHT)-(input==INPUT_LEFT);
            const int expectedRot=(sim.CurrentRotation()+(input==INPUT_ROTATE))%kNumRotations;
            if(probe.CurrentCol()!=expectedCol || probe.CurrentRotation()!=expectedRot) {
                queue_.clear(); gate_.defer(); status_=Status::blocked; return INPUT_NONE;
            }
        }
        queue_.pop_front();
        gate_.defer();
        status_=Status::input;
        return input;
    }
private:
    TickGate gate_;
    int targetCol_=0, targetRot_=0;
    Status status_=Status::waiting;
    bool spawned_=false;
    uint64_t pieceRng_=0;
    std::deque<uint8_t> queue_;
};
}
