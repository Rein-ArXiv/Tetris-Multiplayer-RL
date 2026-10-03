#pragma once

#include <cstdint>
#include <optional>

#include "core/rng.h"
#include "simulation/grid.h"
#include "simulation/session_seed.h"

namespace study_holes {

// Owns a scripted cursor or a seeded engine. Round calls next once per
// positive insertion batch; observers and zero-row requests do not consume it.
class HoleSource {
public:
    static_assert(study_grid::Grid::kColumns == 10,
                  "HoleSource assumes the fixed logical board width of 10");

    // Scripted default: holes {4, 8, 1}, cursor 0, no engine.
    HoleSource() noexcept = default;

    // Derive the garbage seed, then allow the engine's zero normalization.
    static HoleSource seeded(std::uint64_t requested_seed) noexcept {
        HoleSource source;
        source.rng_.emplace(study_session::garbage_seed(requested_seed));
        source.cursor_ = 0u;
        return source;
    }

    // Draw the next hole column. Scripted mode cycles {4, 8, 1}; seeded mode
    // draws a fresh value in [0, 10). Call once per positive batch only.
    unsigned next() noexcept {
        if (rng_.has_value()) {
            return static_cast<unsigned>(rng_->nextUInt(10u));
        }
        const unsigned value = holes_[cursor_];
        cursor_ = (cursor_ + 1u) % 3u;
        return value;
    }

    // Scripted cursor position; always 0 while seeded (the engine drives values).
    unsigned cursor() const noexcept { return cursor_; }

    // Observed engine state; nullopt identifies the scripted mode.
    std::optional<std::uint64_t> rng_state() const noexcept {
        if (rng_.has_value()) {
            return rng_->getState();
        }
        return std::nullopt;
    }

private:
    std::optional<XorShift64Star> rng_{};
    unsigned cursor_ = 0u;
    static constexpr unsigned holes_[3] = {4u, 8u, 1u};
};

} // namespace study_holes
