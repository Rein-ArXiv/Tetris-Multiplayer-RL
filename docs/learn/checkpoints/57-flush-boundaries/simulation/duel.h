#pragma once
#include <cstdint>
#include <optional>
#include "simulation/pending_controls.h"
#include "simulation/round.h"

namespace study_combat {
struct DuelStep {
    study_round::Step left;
    study_round::Step right;
    std::uint64_t left_attack = 0;
    std::uint64_t right_attack = 0;
};

// Single-thread value owner. This coordinates two rounds; it does not decide
// a match winner. The caller can stop the match when either round finishes.
class Duel {
public:
    Duel(const study_round::Round& left, const study_round::Round& right) noexcept
        : left_(left), right_(right), left_delivered_(left.attack_sent()),
          right_delivered_(right.attack_sent()) {}
    const study_round::Round& left() const noexcept { return left_; }
    const study_round::Round& right() const noexcept { return right_; }

    std::optional<DuelStep> tick(const study_input::Intent& li,
                                  const study_input::Intent& ri) noexcept {
        Duel candidate = *this;
        const auto ls = candidate.left_.tick(li.horizontal,li.clockwise,li.soft_drop,li.hard_drop);
        const auto rs = candidate.right_.tick(ri.horizontal,ri.clockwise,ri.soft_drop,ri.hard_drop);
        if (ls == study_round::Step::invalid || rs == study_round::Step::invalid)
            return std::nullopt;
        // Both boards have advanced before either can receive this tick's attack.
        const auto lt = candidate.left_.attack_sent(), rt = candidate.right_.attack_sent();
        if (lt < candidate.left_delivered_ || rt < candidate.right_delivered_)
            return std::nullopt; // Do not wrap an unsigned subtraction after reset.
        const auto ld = lt - candidate.left_delivered_, rd = rt - candidate.right_delivered_;
        constexpr std::uint64_t cap = study_grid::Grid::kRows;
        // Cross the two amounts: a board must never receive its own attack.
        if (ld && !candidate.right_.finished() &&
            !candidate.right_.add_garbage(static_cast<int>(ld > cap ? cap : ld)))
            return std::nullopt;
        if (rd && !candidate.left_.finished() &&
            !candidate.left_.add_garbage(static_cast<int>(rd > cap ? cap : rd)))
            return std::nullopt;
        candidate.left_delivered_ = lt;
        candidate.right_delivered_ = rt;
        *this = candidate;
        return DuelStep{ls,rs,ld,rd}; // Observed total increments, before delivery cap.
    }
private:
    study_round::Round left_, right_;
    std::uint64_t left_delivered_ = 0, right_delivered_ = 0;
};
} // namespace study_combat
