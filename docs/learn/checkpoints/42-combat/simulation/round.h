#pragma once
#include "simulation/catalog.h"
#include "simulation/locking.h"
#include "simulation/lines.h"
#include "simulation/kicks.h"
#include "simulation/next_queue.h"
#include "simulation/scripted_source.h"
#include "simulation/spawn.h"
#include "simulation/score.h"
#include "simulation/ghost.h"
#include "simulation/soft_drop.h"
#include "simulation/combat.h"
#include <optional>

namespace study_round {
enum class EndReason { none, initial_spawn_blocked, spawn_blocked, garbage_overflow };
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
        // A scoring round starts between locks: no unresolved complete row.
        // Four newly locked cells can then complete at most four rows.
        for (int row=0; row<study_grid::Grid::kRows; ++row)
            if (study_lines::row_full(board,row)) return std::nullopt;
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

    // One simulation tick.
    //
    // Order: horizontal shift, then rotation, then advance the soft-drop
    // counter (once per accepted tick). A hard drop then projects the active
    // piece straight down to its landing position and locks it immediately;
    // it does not perform soft movement or natural gravity for this tick.
    //
    // The fourth parameter defaults to false so existing three-argument tick
    // calls keep compiling and behave exactly as before.
    Step tick(int direction, bool clockwise = false, bool soft_drop = false,
              bool hard_drop = false) noexcept {
        if (finished()) return Step::stopped;
        if (!active_) return Step::invalid; // Broken stable-state invariant, not a defeat.
        if (direction < -1 || direction > 1) return Step::invalid;
        Round candidate = *this;
        candidate.last_cleared_ = 0;
        candidate.last_awarded_ = 0;
        candidate.last_rotation_candidate_ = -1;
        candidate.last_hard_drop_distance_ = -1;
        candidate.last_garbage_ = 0;
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
        // Advance the soft-drop counter on every accepted tick. Its phase is
        // owned by the Round, so it persists across a lock and is not reset
        // by spawning the next piece.
        const auto repeat = study_soft_drop::tick(soft_drop,candidate.soft_drop_);
        if (repeat == study_soft_drop::Result::invalid) return Step::invalid;
        Step result = Step::waiting;
        if (hard_drop) {
            // Project to the landing position and lock immediately. Distance
            // zero still locks: an already-resting piece is resolved here too.
            const auto landing = study_ghost::project(candidate.board_, *candidate.active_);
            if (!landing) return Step::invalid;
            candidate.active_ = landing->piece;
            candidate.last_hard_drop_distance_ = landing->distance;
            const auto resolved = finish_lock(candidate);
            if (resolved == Step::invalid) return Step::invalid;
            result = resolved;
        } else {
            auto soft = study_movement::Result::idle;
            if (repeat == study_soft_drop::Result::due)
                soft = study_gravity::try_down(candidate.board_,*candidate.active_);
            if (soft == study_movement::Result::invalid) return Step::invalid;
            // At most one lock per tick. A blocked soft drop resolves this piece;
            // do not spend this tick's gravity on a newly spawned piece.
            const auto fall = soft == study_movement::Result::blocked
                ? study_gravity::TickResult::blocked
                : study_gravity::tick(candidate.board_, *candidate.active_,candidate.gravity_);
            if (fall == study_gravity::TickResult::invalid) return Step::invalid;
            result = (move == study_movement::Result::moved ||
                      fall == study_gravity::TickResult::moved ||
                      soft == study_movement::Result::moved ||
                      turn == study_rotation::Result::rotated) ? Step::changed : Step::waiting;
            if (fall == study_gravity::TickResult::blocked) {
                const auto resolved = finish_lock(candidate);
                if (resolved == Step::invalid) return Step::invalid;
                result = resolved;
            }
        }
        *this = candidate; // All related state becomes the next value together.
        return result;
    }

    // Derived value, not another owner of game state. No clock or queue advances.
    std::optional<study_ghost::Landing> ghost() const noexcept {
        return active_ ? study_ghost::project(board_, *active_) : std::nullopt;
    }

    // Queue an amount, not an immediate board edit. Ended rounds are absorbing.
    bool add_garbage(int rows) noexcept {
        if (finished()) return false;
        const auto pending = study_combat::add_pending(pending_garbage_, rows);
        if (!pending) return false;
        pending_garbage_ = *pending;
        return true;
    }
    std::uint64_t attack_sent() const noexcept { return attack_sent_; }
    int pending_garbage() const noexcept { return pending_garbage_; }
    int last_garbage() const noexcept { return last_garbage_; }
    unsigned hole_cursor() const noexcept { return hole_cursor_; }

    const study_grid::Grid& board() const noexcept { return board_; }
    const std::optional<study_piece::Piece>& active() const noexcept { return active_; }
    const study_soft_drop::Counter& soft_drop() const noexcept { return soft_drop_; }
    const study_gravity::Counter& gravity() const noexcept { return gravity_; }
    // Accepted rotation in this tick, including index zero. Not a persistent event queue.
    int last_rotation_candidate() const noexcept { return last_rotation_candidate_; }
    // Rows travelled by the most recent accepted hard drop. -1 when
    // that tick was not a hard drop. Not a score and not an event queue.
    int last_hard_drop_distance() const noexcept { return last_hard_drop_distance_; }
    const study_next::Queue& next() const noexcept { return next_; }
    study_catalog::Kind kind() const noexcept { return kind_; }
    std::size_t source_cursor() const noexcept { return source_.cursor(); }
    int quarter() const noexcept { return quarter_; }
    EndReason end_reason() const noexcept { return end_reason_; }
    bool finished() const noexcept { return end_reason_!=EndReason::none; }
    // Result of the most recent accepted tick, not a consumable event queue.
    int last_cleared() const noexcept { return last_cleared_; }
    std::uint64_t score() const noexcept { return totals_.points; }
    std::uint64_t total_lines() const noexcept { return totals_.lines; }
    unsigned level() const noexcept { return study_score::level(totals_); }
    // Actual stored increase of the most recent accepted tick, not a command.
    std::uint64_t last_awarded() const noexcept { return last_awarded_; }
private:
    explicit Round(study_next::ScriptedSource source) noexcept : source_(source) {}

    // Resolve a piece that can no longer fall: lock it into the board, clear
    // complete rows, score, advance the queue, and attempt the next spawn.
    // Mutates only the supplied candidate, never *this; the caller publishes
    // the candidate only when this returns a non-invalid Step.
    static Step finish_lock(Round& candidate) noexcept {
        if (study_locking::try_lock(candidate.board_, *candidate.active_) !=
            study_locking::Result::locked) return Step::invalid;
        candidate.active_.reset(); // The locked piece now belongs to the board.
        // Resolve the board before testing the next spawn.
        candidate.last_cleared_ = study_lines::clear_full_rows(candidate.board_);
        const auto scored = study_score::award(candidate.totals_,candidate.last_cleared_);
        if (!scored) return Step::invalid;
        const auto old_level = candidate.level();
        candidate.last_awarded_ = scored->points - candidate.totals_.points;
        candidate.totals_ = *scored;
        if (candidate.level() != old_level) {
            const auto interval = study_score::gravity_interval(candidate.level());
            if (!interval) return Step::invalid;
            candidate.gravity_.interval = *interval;
        }
        const auto attack = study_combat::normal_attack(candidate.last_cleared_);
        if (!attack) return Step::invalid;
        candidate.attack_sent_ = study_score::saturating_add(candidate.attack_sent_,
                                                           static_cast<std::uint64_t>(*attack));
        bool overflow = false;
        if (candidate.pending_garbage_ > 0) {
            // Scripted exercise source, not a random generator. One hole per
            // insertion batch, even when several deliveries were accumulated.
            constexpr int holes[] = {4, 8, 1};
            const auto inserted = study_combat::insert(candidate.board_,candidate.pending_garbage_,
                                                       holes[candidate.hole_cursor_]);
            if (!inserted) return Step::invalid;
            candidate.board_ = inserted->board;
            candidate.last_garbage_ = inserted->rows;
            candidate.pending_garbage_ = 0;
            candidate.hole_cursor_ = (candidate.hole_cursor_ + 1) % 3;
            overflow = inserted->overflow;
        }
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
        // Queue advancement is recorded even when the final board ends the round.
        if (overflow) {
            candidate.end_reason_ = EndReason::garbage_overflow;
            return Step::game_over;
        }
        const auto placement=study_spawn::assess(candidate.board_,*spawn);
        if(placement==study_spawn::Status::invalid)return Step::invalid;
        if (placement==study_spawn::Status::ready) {
            candidate.active_ = spawn;
            return Step::locked;
        }
        candidate.end_reason_=EndReason::spawn_blocked;
        return Step::game_over; // Keep the resolved board, no active overlay.
    }

    std::uint64_t attack_sent_ = 0;
    int pending_garbage_ = 0;
    int last_garbage_ = 0;
    unsigned hole_cursor_ = 0;
    EndReason end_reason_=EndReason::none;
    study_score::Totals totals_;
    std::uint64_t last_awarded_=0;
    study_next::ScriptedSource source_;
    study_next::Queue next_;
    study_grid::Grid board_;
    std::optional<study_piece::Piece> active_;
    study_gravity::Counter gravity_;
    study_soft_drop::Counter soft_drop_;
    int last_rotation_candidate_ = -1;
    int last_hard_drop_distance_ = -1;
    int quarter_ = 0;
    int last_cleared_ = 0;
    study_catalog::Kind kind_ = study_catalog::Kind::T;
};
} // namespace study_round
