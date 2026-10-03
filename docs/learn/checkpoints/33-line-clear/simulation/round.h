#pragma once
#include "simulation/catalog.h"
#include "simulation/locking.h"
#include "simulation/lines.h"
#include <optional>

namespace study_round {
enum class Step { waiting, moved, locked, game_over, stopped, invalid };

// Own the related state together. External readers cannot edit it halfway
// through a transition. This value is for one thread, not concurrent access.
class Round {
public:
    static std::optional<Round> create(const study_grid::Grid& board,
                                      study_catalog::Kind kind,
                                      int interval = 30) noexcept {
        const auto spawn = study_catalog::make_piece(kind);
        if (!spawn || interval <= 0) return std::nullopt;
        Round round;
        round.board_ = board;
        round.kind_ = kind;
        round.gravity_.interval = interval;
        if (study_collision::classify(board, *spawn) == study_collision::Placement::clear)
            round.active_ = spawn;
        // A known piece whose spawn is occupied creates a finished round.
        return round;
    }

    Step tick(int direction) noexcept {
        if (!active_) return Step::stopped;
        if (direction < -1 || direction > 1) return Step::invalid;
        Round candidate = *this;
        candidate.last_cleared_ = 0;
        const auto move = study_collision::try_shift(candidate.board_,
                                                     *candidate.active_, direction);
        if (move == study_movement::Result::invalid) return Step::invalid;
        const auto fall = study_gravity::tick(candidate.board_, *candidate.active_,
                                              candidate.gravity_);
        if (fall == study_gravity::TickResult::invalid) return Step::invalid;
        Step result = (move == study_movement::Result::moved ||
                       fall == study_gravity::TickResult::moved) ? Step::moved : Step::waiting;
        if (fall == study_gravity::TickResult::blocked) {
            if (study_locking::try_lock(candidate.board_, *candidate.active_) !=
                study_locking::Result::locked) return Step::invalid;
            candidate.active_.reset(); // The locked piece now belongs to the board.
            // Resolve the board before testing the next spawn.
            candidate.last_cleared_ = study_lines::clear_full_rows(candidate.board_);
            const auto spawn = study_catalog::make_piece(candidate.kind_);
            if (!spawn) return Step::invalid;
            candidate.gravity_.elapsed = 0;
            if (study_collision::classify(candidate.board_, *spawn) ==
                study_collision::Placement::clear) {
                candidate.active_ = spawn;
                result = Step::locked;
            } else {
                result = Step::game_over; // Keep the resolved board, no active overlay.
            }
        }
        *this = candidate; // All related state becomes the next value together.
        return result;
    }

    const study_grid::Grid& board() const noexcept { return board_; }
    const std::optional<study_piece::Piece>& active() const noexcept { return active_; }
    const study_gravity::Counter& gravity() const noexcept { return gravity_; }
    bool finished() const noexcept { return !active_; }
    // Result of the most recent accepted tick, not a consumable event queue.
    int last_cleared() const noexcept { return last_cleared_; }
private:
    Round() = default;
    study_grid::Grid board_;
    std::optional<study_piece::Piece> active_;
    study_gravity::Counter gravity_;
    int last_cleared_ = 0;
    study_catalog::Kind kind_ = study_catalog::Kind::T;
};
} // namespace study_round
