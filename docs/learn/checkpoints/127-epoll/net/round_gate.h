#ifndef STUDY_NET_ROUND_GATE_H
#define STUDY_NET_ROUND_GATE_H

// Educational gate for local round lifecycle transitions in study_net.
// A single main owner drives this object; it holds no locks and assumes no
// concurrency. It gates only local operations and adds no clocks or timers.
// Local UI readiness is not the same as simultaneous peer readiness.
//
// Same-round remote input can be classified as current during the local
// countdown so it may be staged; local capture stays barred until playing.

#include <cstdint>

namespace study_net {

enum class RoundPhase { waiting, countdown, playing, ended };

// invalid_frame is produced by the codec integration, not classify().
enum class RoundScope { invalid_frame, inactive, old_round, current, future_round };

class RoundGate {
 public:
  RoundGate() noexcept = default;

  // Begin a new local round. Only from waiting or ended, never for round zero
  // or a non-advancing id. Ids never wrap, so once round_ is the maximum value
  // no later prepare can succeed. An incoming future id never resets locally.
  bool prepare(std::uint64_t id) noexcept {
    if (phase_ != RoundPhase::waiting && phase_ != RoundPhase::ended) {
      return false;
    }
    if (id == 0 || id <= round_) {
      return false;
    }
    round_ = id;
    phase_ = RoundPhase::countdown;
    return true;
  }

  // countdown -> playing only.
  bool start() noexcept {
    if (phase_ != RoundPhase::countdown) {
      return false;
    }
    phase_ = RoundPhase::playing;
    return true;
  }

  // countdown or playing -> ended only.
  bool finish() noexcept {
    if (phase_ != RoundPhase::countdown && phase_ != RoundPhase::playing) {
      return false;
    }
    phase_ = RoundPhase::ended;
    return true;
  }

  // Local capture is permitted only while playing.
  bool can_capture() const noexcept { return phase_ == RoundPhase::playing; }

  // Classify an id relative to the local round without mutating state.
  //   id < round_  -> old_round (checked before the phase)
  //   id > round_  -> future_round
  //   id == round_ != 0 while countdown or playing -> current
  //   otherwise    -> inactive
  RoundScope classify(std::uint64_t id) const noexcept {
    if (id < round_) {
      return RoundScope::old_round;
    }
    if (id > round_) {
      return RoundScope::future_round;
    }
    if (id != 0 &&
        (phase_ == RoundPhase::countdown || phase_ == RoundPhase::playing)) {
      return RoundScope::current;
    }
    return RoundScope::inactive;
  }

  std::uint64_t round() const noexcept { return round_; }
  RoundPhase phase() const noexcept { return phase_; }

 private:
  std::uint64_t round_{0};
  RoundPhase phase_{RoundPhase::waiting};
};

}  // namespace study_net

#endif  // STUDY_NET_ROUND_GATE_H
