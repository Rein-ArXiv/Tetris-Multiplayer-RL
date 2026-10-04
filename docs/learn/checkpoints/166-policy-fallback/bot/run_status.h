#ifndef BOT_RUN_STATUS_HPP
#define BOT_RUN_STATUS_HPP

namespace bot {

// Local run-status latch. This models gameplay/policy status only.
// It is not an authentication or authorization mechanism; server-side
// authorization is outside the scope of this helper.
enum class Mode { practice, reward };
enum class Fault { none, policy_failed, invalid_target };

class RunStatus {
public:
    // Start a fresh run in the given mode, clearing any stored fault.
    void reset(Mode mode) noexcept {
        mode_ = mode;
        fault_ = Fault::none;
    }

    // Store the FIRST non-none fault permanently until reset().
    // Observing Fault::none never changes state and never restores eligibility.
    void observe(Fault fault) noexcept {
        if (fault == Fault::none) {
            return;
        }
        if (fault_ == Fault::none) {
            fault_ = fault;
        }
    }

    // Eligible only in reward mode with no recorded fault.
    bool reward_eligible() const noexcept {
        return mode_ == Mode::reward && fault_ == Fault::none;
    }

    // Degraded if any fault has been recorded.
    bool degraded() const noexcept {
        return fault_ != Fault::none;
    }

    Fault fault() const noexcept {
        return fault_;
    }

private:
    Mode mode_ = Mode::practice;
    Fault fault_ = Fault::none;
};

}  // namespace bot

#endif  // BOT_RUN_STATUS_HPP
