#pragma once
#include "client/game.h"
#include "client/screen_transition.h"
#include <cmath>

namespace study_app {
struct AppReport {
    bool screen_changed = false;
    bool game_replaced = false;
    std::optional<study_loop::FrameReport> frame;
};

// Owns one immutable restart baseline and, outside the menu, one current game.
// Single-threaded value object. Borrowed game pointers expire on transitions.
class Application {
public:
    explicit Application(study_round::Round initial) noexcept : initial_(initial) {}
    Screen screen() const noexcept { return screen_; }
    const study_game::Game* game() const noexcept { return game_ ? &*game_ : nullptr; }
    bool controls_armed() const noexcept { return armed_; }

    // confirm/back are edges. released means ALL gameplay keys are currently up.
    std::optional<AppReport> advance(double seconds, study_loop::FrameInput input,
                                    bool confirm, bool back, bool released) noexcept {
        if (!std::isfinite(seconds) || seconds < 0) return std::nullopt;
        const auto command = back ? Command::back : confirm ? Command::confirm : Command::none;
        const auto decision = transition(screen_, command);
        AppReport report;
        if (decision.release) {
            game_.reset();
            armed_ = false;
            report.screen_changed = decision.next != screen_;
            screen_ = decision.next;
            return report; // The departure frame never advances a round.
        }
        if (decision.recreate) {
            game_.emplace(initial_); // Resets rule, pending input, phase and effects.
            armed_ = false;
            const auto next = game_->round().end_reason() != study_round::EndReason::none ? Screen::finished : Screen::playing;
            report.screen_changed = next != screen_;
            report.game_replaced = true;
            screen_ = next;
            return report; // The confirm edge is not also a hard drop.
        }
        if (screen_ != Screen::playing) return report;

        bool next_armed = armed_;
        if (input.cancelled) next_armed = false;
        if (!next_armed) {
            const bool cancelled = input.cancelled;
            input = {};
            input.cancelled = cancelled; // Clear earlier zero-tick requests on focus loss.
            if (released && !cancelled) next_armed = true;
        }
        const auto frame = game_->advance(seconds, input);
        if (!frame) return std::nullopt; // Game and the input gate retain their old state.
        armed_ = next_armed;
        report.frame = *frame;
        if (game_->round().end_reason() != study_round::EndReason::none) {
            screen_ = Screen::finished;
            report.screen_changed = true;
        }
        return report;
    }
private:
    study_round::Round initial_;
    std::optional<study_game::Game> game_;
    Screen screen_ = Screen::menu;
    bool armed_ = false;
};
} // namespace study_app
