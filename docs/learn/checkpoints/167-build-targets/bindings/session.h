#pragma once

// Own a round at the language boundary; reuse its rules and input decoder.

#include <cstdint>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

#include "simulation/input_mask.h"
#include "simulation/state_hash.h"
#include "simulation/action_plan.h"

namespace study_python {

// Owns one Round value; every mutation is copy-then-commit.
class Session {
public:
    // Validate the interval before constructing any round state.
    explicit Session(std::uint64_t seed, int gravity_interval = 30)
        : round_(make_round(seed, gravity_interval)) {}

    // Rebuild from a fresh seeded Round; commit only after creation succeeds.
    void reset(std::uint64_t seed, int gravity_interval) {
        round_ = make_round(seed, gravity_interval);
    }

    // Value copy of the owned round and all of its state.
    Session clone() const { return *this; }

    // Decode first, then tick a candidate; never commit a rejected intent.
    study_round::Step step(unsigned mask) {
        const auto intent = study_input::decode(mask);
        if (!intent) {
            throw std::invalid_argument("Session::step: unknown input mask bits");
        }
        study_round::Round candidate = round_;
        const auto result = candidate.tick(intent->horizontal, intent->clockwise,
                                           intent->soft_drop, intent->hard_drop);
        if (result == study_round::Step::invalid) {
            throw std::runtime_error("Session::step: round rejected intent");
        }
        round_ = std::move(candidate); // commit only on a valid result
        return result;                 // Step::stopped is a valid outcome
    }

    // Native match coordination; this view is not exported to Python.
    const study_round::Round& round() const noexcept { return round_; }

    // Copies the selected features in one owner-thread call. Other state stays private.
    struct Snapshot {
        std::vector<std::vector<int>> board;
        int current_id;
        int next_id;
    };
    Snapshot snapshot() const {
        const auto next = round_.next().peek();
        if (!next) throw std::runtime_error("observation: missing next piece");
        return {grid(), static_cast<int>(round_.kind()), static_cast<int>(*next)};
    }
    static std::vector<int> piece_ids() {
        std::vector<int> result;
        for (const auto& entry : study_catalog::definitions)
            result.push_back(static_cast<int>(entry.kind));
        return result;
    }

    struct Pose { int column; int quarter; };
    std::optional<Pose> pose() const noexcept {
        if(!round_.active())return std::nullopt;
        return Pose{round_.active()->origin.column,round_.quarter()};
    }

    std::vector<int> legal_actions() const { return study_actions::legal_actions(round_); }
    std::vector<study_input::Mask> action_trace(int action) const {
        const auto selected=study_actions::plan(round_,action);
        if(!selected)throw std::invalid_argument("action has no route from this state");
        return selected->inputs;
    }
    struct ActionResult { std::size_t ticks; int lines; bool ended; };
    ActionResult apply_action(int action) {
        const auto selected=study_actions::plan(round_,action);
        if(!selected)throw std::invalid_argument("action has no route from this state");
        round_=selected->result;
        return {selected->inputs.size(),round_.last_cleared(),round_.finished()};
    }

    void add_garbage(int rows) {
        auto candidate = round_;
        if (!candidate.add_garbage(rows))
            throw std::invalid_argument("garbage requires a live round and nonnegative rows");
        round_ = std::move(candidate);
    }
    std::uint64_t attack_sent() const noexcept { return round_.attack_sent(); }
    int pending_garbage() const noexcept { return round_.pending_garbage(); }
    int last_garbage() const noexcept { return round_.last_garbage(); }

    bool finished() const noexcept { return round_.finished(); }

    std::uint64_t score() const noexcept { return round_.score(); }

    // Canonical hash bytes; surface encoder overflow as a runtime error.
    std::vector<std::uint8_t> state_bytes() const {
        const auto state = study_hash::state_bytes(round_);
        if (!state.ok()) {
            throw std::runtime_error("Session::state_bytes: encoder overflow");
        }
        return std::vector<std::uint8_t>(state.data(), state.data() + state.size());
    }

    // Copy the board cell by cell using the Grid's compile-time dimensions.
    std::vector<std::vector<int>> grid() const {
        const auto& cells = round_.board().cells();
        std::vector<std::vector<int>> out(
            static_cast<std::size_t>(study_grid::Grid::kRows),
            std::vector<int>(static_cast<std::size_t>(study_grid::Grid::kColumns)));
        for (int row = 0; row < study_grid::Grid::kRows; ++row) {
            for (int column = 0; column < study_grid::Grid::kColumns; ++column) {
                out[static_cast<std::size_t>(row)][static_cast<std::size_t>(column)] =
                    static_cast<int>(cells[static_cast<std::size_t>(
                        row * study_grid::Grid::kColumns + column)]);
            }
        }
        return out;
    }

private:
    // Reject non-positive intervals, then build a seeded round or fail cleanly.
    static study_round::Round make_round(std::uint64_t seed, int gravity_interval) {
        if (gravity_interval <= 0) {
            throw std::invalid_argument("Session: gravity interval must be positive");
        }
        const auto created =
            study_round::Round::create_seeded(study_grid::Grid{}, seed, gravity_interval);
        if (!created) {
            throw std::runtime_error("Session: could not create seeded round");
        }
        return *created;
    }

    study_round::Round round_;
};

} // namespace study_python