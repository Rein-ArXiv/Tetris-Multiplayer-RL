#pragma once
#include "loop/frame_runner.h"
#include "presentation/game_view.h"
#include "presentation/accent_noise.h"
#include "renderer/blend.h"

namespace study_game {
// Value-owned CPU adapter. Copies fork the runner AND presentation state.
// Device resources stay in the render scope, so no custom destructor is needed.
class Game {
public:
    explicit Game(study_round::Round round, std::uint64_t visual_seed = 17) noexcept
        : runner_(round), accent_(visual_seed) {}

    std::optional<study_loop::FrameReport> advance(double seconds,
                                                 study_loop::FrameInput input) noexcept {
        auto report = runner_.advance(seconds, input);
        if (!report) return std::nullopt;
        // Consume each new report exactly once here. View/draw never consume it.
        for (unsigned i = 0; i < report->ticks; ++i) {
            if (report->observations[i].lock)
                background_.b = 0.09375 + 0.002 * accent_.sample();
        }
        return report;
    }

    std::optional<GameView> view() const noexcept { return make_view(runner_.round()); }
    // Borrowed rule state for diagnostics; invalid when this Game is destroyed.
    const study_round::Round& round() const noexcept { return runner_.round(); }
    study_blend::Rgba background() const noexcept { return background_; }
    std::uint64_t accent_state() const noexcept { return accent_.state(); }
    std::uint64_t phase() const noexcept { return runner_.phase(); }

private:
    study_loop::FrameRunner runner_;
    study_presentation::AccentNoise accent_;
    study_blend::Rgba background_{0.03125, 0.0625, 0.09375, 1};
};
} // namespace study_game
