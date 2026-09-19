#pragma once
#include "../src/sim_game.h"
#include "../net/framing.h"
#include "../net/match_result.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <vector>

namespace relay {
struct VerifiedResult {
    net::ResultStatus status = net::ResultStatus::Incomplete;
    int winner = 0; // 1=HOST, 2=GUEST, 0=no winner. Never a player ID.
    int score_a = 0, score_b = 0, lines_a = 0, lines_b = 0, duration_s = 0;
};

// Both relay implementations use this authority. Call under the channel lock in
// the thread relay; reactor channels are owned by one loop. No renderer or HTTP.
class RankedGame {
  public:
    using Clock = std::chrono::steady_clock;
    static constexpr uint32_t max_ticks = 60 * 30 * 60; // bounded 30-minute round
    explicit RankedGame(uint64_t seed) : seed_(seed), a_(seed), b_(seed) {}

    void observe(int side, net::MsgType type, const uint8_t *data, size_t size,
                 Clock::time_point now = Clock::now()) {
        if (!valid_)
            return;
        if (side != 1 && side != 2) {
            valid_ = false;
            return;
        }
        if (type == net::MsgType::SEED) {
            // Initial host setup may announce the server seed; it cannot choose a
            // replacement. A ranked rematch needs a new connection/UUID/seed.
            if (side != 1 || size != 14 || net::le_read_u64(data) != seed_ || ticks_ != 0)
                valid_ = false;
            return;
        }
        if (type != net::MsgType::INPUT)
            return;
        if (size < 6) {
            valid_ = false;
            return;
        }
        const uint32_t from = net::le_read_u32(data);
        const uint16_t count = net::le_read_u16(data + 4);
        if (!count || size != size_t(count) + 6 || count > 120 || from > max_ticks ||
            count > max_ticks - from) {
            valid_ = false;
            return;
        }
        if (!started_) {
            started_ = true;
            began_ = now;
        }
        const auto elapsed = std::chrono::duration<double>(now - began_).count();
        if (double(from) + count > elapsed * 60.0 + 180.0) {
            valid_ = false;
            return;
        }
        auto &history = inputs_[side - 1];
        if (from > history.size() || from + count > ticks_ + 4096) {
            valid_ = false;
            return;
        }
        for (uint32_t i = 0; i < count; ++i) {
            const auto mask = data[6 + i];
            const auto tick = from + i;
            if (mask & ~uint8_t(31)) {
                valid_ = false;
                return;
            }
            if (tick < history.size()) {
                if (history[tick] != mask) {
                    valid_ = false;
                    return;
                }
            } else
                history.push_back(mask);
        }
        // Advance each tick exactly once, stopping at the first terminal board.
        while (!finished_ && ticks_ < std::min(inputs_[0].size(), inputs_[1].size())) {
            a_.SubmitInput(inputs_[0][ticks_]);
            b_.SubmitInput(inputs_[1][ticks_]);
            a_.Tick();
            b_.Tick();
            const int aa = a_.AttackLinesSent(), ab = b_.AttackLinesSent();
            b_.AddPendingGarbage(aa - attack_a_);
            a_.AddPendingGarbage(ab - attack_b_);
            attack_a_ = aa;
            attack_b_ = ab;
            ++ticks_;
            finished_ = a_.IsGameOver() || b_.IsGameOver();
        }
    }
    void invalidate() {
        valid_ = false;
    }
    VerifiedResult result() const {
        VerifiedResult out;
        if (!valid_) {
            out.status = net::ResultStatus::InvalidReplay;
            return out;
        }
        if (!finished_)
            return out;
        out.winner = a_.IsGameOver() == b_.IsGameOver() ? 0 : a_.IsGameOver() ? 2 : 1;
        out.status = out.winner ? net::ResultStatus::Applied : net::ResultStatus::Draw;
        out.score_a = a_.Score();
        out.score_b = b_.Score();
        out.lines_a = a_.totalLinesCleared;
        out.lines_b = b_.totalLinesCleared;
        out.duration_s = static_cast<int>((ticks_ + 59) / 60);
        return out;
    }

  private:
    uint64_t seed_;
    SimGame a_, b_;
    std::array<std::vector<uint8_t>, 2> inputs_;
    uint32_t ticks_ = 0;
    int attack_a_ = 0, attack_b_ = 0;
    bool valid_ = true, started_ = false, finished_ = false;
    Clock::time_point began_{};
};
} // namespace relay
