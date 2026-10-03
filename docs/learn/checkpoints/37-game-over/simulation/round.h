#pragma once
#include "simulation/catalog.h"
#include "simulation/locking.h"
#include "simulation/lines.h"
#include "simulation/kicks.h"
#include "simulation/next_queue.h"
#include "simulation/scripted_source.h"
#include "simulation/spawn.h"
#include <optional>

namespace study_round {
enum class EndReason { none, initial_spawn_blocked, spawn_blocked };
enum class Step { waiting, changed, locked, game_over, stopped, invalid };

// Own the related state together. External readers cannot edit it halfway
// through a transition. This value is for one thread, not concurrent access.
class Round {
public:
    static std::optional<Round> create(const study_grid::Grid& board,
                                      study_catalog::Kind kind,
                                      int interval = 30) noexcept {
        const auto source=study_next::ScriptedSource::repeating(kind);
        return source ? create(board,*source,interval) : std::nullopt;
    }

    // Own a value copy of the producer and prefill three upcoming kinds.
    static std::optional<Round> create(const study_grid::Grid& board,
                                      study_next::ScriptedSource source,
                                      int interval = 30) noexcept {
        if(interval<=0)return std::nullopt;
        Round round(source);
        round.board_=board;
        round.gravity_.interval=interval;
        round.kind_=round.source_.next();
        const auto spawn=study_catalog::make_piece(round.kind_);
        if(!spawn)return std::nullopt;
        for(std::size_t i=0;i<study_next::Queue::capacity;++i)
            if(!round.next_.push(round.source_.next()))return std::nullopt;
        const auto placement=study_spawn::assess(board,*spawn);
        if(placement==study_spawn::Status::invalid)return std::nullopt;
        if(placement==study_spawn::Status::ready)round.active_=spawn;
        else round.end_reason_=EndReason::initial_spawn_blocked;
        // Initial collision is a valid, finished round with a full preview.
        return round;
    }

    Step tick(int direction, bool clockwise = false) noexcept {
        if (finished()) return Step::stopped;
        if (!active_) return Step::invalid; // Broken stable-state invariant, not a defeat.
        if (direction < -1 || direction > 1) return Step::invalid;
        Round candidate = *this;
        candidate.last_cleared_ = 0;
        candidate.last_rotation_candidate_ = -1;
        const auto move = study_collision::try_shift(candidate.board_,
                                                     *candidate.active_, direction);
        if (move == study_movement::Result::invalid) return Step::invalid;
        auto turn = study_rotation::Result::blocked; // No requested shape change.
        if (clockwise) {
            const auto rotation = study_kicks::try_clockwise(candidate.board_,*candidate.active_,
                                                             candidate.kind_,candidate.quarter_);
            turn = rotation.result;
            candidate.last_rotation_candidate_ = rotation.candidate_index;
            if (turn == study_rotation::Result::invalid) return Step::invalid;
        }
        const auto fall = study_gravity::tick(candidate.board_, *candidate.active_,
                                              candidate.gravity_);
        if (fall == study_gravity::TickResult::invalid) return Step::invalid;
        Step result = (move == study_movement::Result::moved ||
                       fall == study_gravity::TickResult::moved ||
                       turn == study_rotation::Result::rotated) ? Step::changed : Step::waiting;
        if (fall == study_gravity::TickResult::blocked) {
            if (study_locking::try_lock(candidate.board_, *candidate.active_) !=
                study_locking::Result::locked) return Step::invalid;
            candidate.active_.reset(); // The locked piece now belongs to the board.
            // Resolve the board before testing the next spawn.
            candidate.last_cleared_ = study_lines::clear_full_rows(candidate.board_);
            const auto next_kind=candidate.next_.peek();
            if(!next_kind)return Step::invalid;
            const auto spawn=study_catalog::make_piece(*next_kind);
            if(!spawn)return Step::invalid;
            // A spawn attempt consumes the selected kind, even if its placement
            // is blocked. Refill before publishing the finished state as well.
            if(!candidate.next_.pop() || !candidate.next_.push(candidate.source_.next()))
                return Step::invalid;
            candidate.kind_=*next_kind;
            candidate.gravity_.elapsed = 0;
            candidate.quarter_ = 0;
            const auto placement=study_spawn::assess(candidate.board_,*spawn);
            if(placement==study_spawn::Status::invalid)return Step::invalid;
            if (placement==study_spawn::Status::ready) {
                candidate.active_ = spawn;
                result = Step::locked;
            } else {
                candidate.end_reason_=EndReason::spawn_blocked;
                result = Step::game_over; // Keep the resolved board, no active overlay.
            }
        }
        *this = candidate; // All related state becomes the next value together.
        return result;
    }

    const study_grid::Grid& board() const noexcept { return board_; }
    const std::optional<study_piece::Piece>& active() const noexcept { return active_; }
    const study_gravity::Counter& gravity() const noexcept { return gravity_; }
    // Accepted rotation in this tick, including index zero. Not a persistent event queue.
    int last_rotation_candidate() const noexcept { return last_rotation_candidate_; }
    const study_next::Queue& next() const noexcept { return next_; }
    study_catalog::Kind kind() const noexcept { return kind_; }
    std::size_t source_cursor() const noexcept { return source_.cursor(); }
    int quarter() const noexcept { return quarter_; }
    EndReason end_reason() const noexcept { return end_reason_; }
    bool finished() const noexcept { return end_reason_!=EndReason::none; }
    // Result of the most recent accepted tick, not a consumable event queue.
    int last_cleared() const noexcept { return last_cleared_; }
private:
    explicit Round(study_next::ScriptedSource source) noexcept : source_(source) {}
    EndReason end_reason_=EndReason::none;
    study_next::ScriptedSource source_;
    study_next::Queue next_;
    study_grid::Grid board_;
    std::optional<study_piece::Piece> active_;
    study_gravity::Counter gravity_;
    int last_rotation_candidate_ = -1;
    int quarter_ = 0;
    int last_cleared_ = 0;
    study_catalog::Kind kind_ = study_catalog::Kind::T;
};
} // namespace study_round
