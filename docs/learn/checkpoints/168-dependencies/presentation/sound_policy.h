#pragma once

// presentation/sound_policy.h - bounded presentation policy for study cues.
//
// Control and destruction stay on the initiating main thread. This policy
// stores no OS handles and writes no logs itself: Device owns PCM and may
// allocate or call the OS in open/replace/play/stop/close. Device's default
// construction, play, stop and close must not throw. Cue creation may throw.
// The backend must finish worker reads before releasing its PCM in close.
// Stopping queued work does not guarantee the speaker is instantly silent.
//
// Acceptance vs. hearing: a Delivery of started means the backend accepted the
// cue, not that the listener heard it - the mixer may be muted, busy, or the
// physical output may be absent.
//
// Failure notices: pending_ records failures not yet reported, while seen_
// (once_flags::Flags) makes each Failure kind report at most once for the
// whole session. That is a teaching once-per-session guarantee, not a rate
// limiter: new play events still reach a ready Device, while prepare attempts
// are latched after the first result.

#include "audio/cue_pcm.h"
#include "core/once_flags.h"
#include "presentation/sound_events.h"

#include <array>
#include <cstddef>
#include <new>
#include <optional>
#include <utility>

namespace study_sound {

enum class AudioState { unprepared, disabled, ready, unavailable };

enum class Failure {
    open,
    clip,
    install,
    memory,
    unexpected,
    play,
    invalid_kind,
    count
};

enum class Delivery { started, skipped, failed, invalid_kind };

// A cue factory is a plain function pointer so a session can be prepared
// without capturing state or allocating a callable. Returning nullopt means
// "this kind has no cue". study_audio::make_cue is the default.
using CueFactory = std::optional<study_audio::Pcm16> (*)(Kind);

// Fixed English diagnostics with static storage duration: no allocation, and
// the returned pointer stays valid for the life of the program.
inline const char* failure_message(Failure failure) noexcept {
    switch (failure) {
        case Failure::open:         return "audio device could not be opened";
        case Failure::clip:         return "audio cue could not be prepared";
        case Failure::install:      return "audio cue could not be installed";
        case Failure::memory:       return "audio preparation ran out of memory";
        case Failure::unexpected:   return "audio preparation failed unexpectedly";
        case Failure::play:         return "audio playback was rejected";
        case Failure::invalid_kind: return "sound kind is out of range";
        case Failure::count:        break;
    }
    return "unknown audio failure";
}

template <class Device, bool Supported>
class BasicSession {
public:
    static constexpr bool supported = Supported;

    BasicSession() noexcept = default;
    BasicSession(const BasicSession&) = delete;
    BasicSession& operator=(const BasicSession&) = delete;
    BasicSession(BasicSession&&) = delete;
    BasicSession& operator=(BasicSession&&) = delete;

    // Closes the backend when this build supports audio. Device is expected to
    // tolerate repeated close after failed prepare and in its own destructor.
    ~BasicSession() noexcept {
        if constexpr (Supported) {
            player_.close();
        }
    }

    // Become ready exactly once for this session object. A second call never
    // retries; it just reports whether this object already reached ready. An
    // unsupported build becomes disabled and a failed supported build becomes
    // unavailable, and neither can be revived - construct a new BasicSession.
    bool prepare(CueFactory factory = &study_audio::make_cue) noexcept {
        if (state_ != AudioState::unprepared) return state_ == AudioState::ready;

        if constexpr (!Supported) {
            // Unsupported build: no error notice and no Device calls at all.
            state_ = AudioState::disabled;
            return false;
        } else {
            // A null factory cannot produce any clip, so it fails like a
            // factory that returns nullopt; do not open the device first.
            if (factory == nullptr) return fail_prepare(Failure::clip);

            try {
                if (!player_.open(44100, 2)) return fail_prepare(Failure::open);

                for (std::size_t slot = 0; slot < kCueCount; ++slot) {
                    std::optional<study_audio::Pcm16> cue =
                        factory(static_cast<Kind>(slot));
                    if (!cue) return fail_prepare(Failure::clip);
                    if (!player_.replace(std::move(*cue), slot))
                        return fail_prepare(Failure::install);
                }
            } catch (const std::bad_alloc&) {
                return fail_prepare(Failure::memory);
            } catch (...) {
                return fail_prepare(Failure::unexpected);
            }

            // Ready only when all four cues were installed.
            state_ = AudioState::ready;
            return true;
        }
    }

    AudioState state() const noexcept { return state_; }

    // Report the first un-reported failure in enum order and clear it. The
    // matching seen_ slot is never cleared, so each kind surfaces at most once
    // per session.
    std::optional<Failure> take_notice() noexcept {
        for (std::size_t i = 0; i < kFailureCount; ++i) {
            if (pending_[i]) {
                pending_[i] = false;
                return static_cast<Failure>(i);
            }
        }
        return std::nullopt;
    }

    // Send one cue. The kind is range-checked before readiness, so an invalid
    // value is always reported as invalid_kind and a ready device is left
    // untouched. Otherwise, while not ready, valid kinds are skipped without
    // any backend call. started means the backend accepted the cue, not that
    // it was heard.
    Delivery send(Kind kind) noexcept {
        const auto index = static_cast<std::size_t>(kind);
        if (static_cast<int>(kind) < 0 || index >= kCueCount) {
            notice_once(Failure::invalid_kind);
            return Delivery::invalid_kind;
        }

        if (state_ != AudioState::ready) return Delivery::skipped;

        if constexpr (Supported) {
            if (!player_.play(index)) {
                // This request failed. A different future cue may still succeed.
                notice_once(Failure::play);
                return Delivery::failed;
            }
            return Delivery::started;
        } else {
            return Delivery::skipped;
        }
    }

    bool play(Kind kind) noexcept { return send(kind) == Delivery::started; }

    // Silence output without releasing prepared cues or changing state. The
    // backend is asked to stop only when the session is ready.
    void stop() noexcept {
        if constexpr (Supported) {
            if (state_ == AudioState::ready) player_.stop();
        }
    }

private:
    static constexpr std::size_t kCueCount = 4;
    static constexpr std::size_t kFailureCount =
        static_cast<std::size_t>(Failure::count);

    // Shared tail for every supported prepare failure: record the notice,
    // remember the session can no longer become ready, and close whatever part
    // of the Device was opened. Device handles repeated close.
    bool fail_prepare(Failure failure) noexcept {
        notice_once(failure);
        state_ = AudioState::unavailable;
        player_.close();
        return false;
    }

    // Record a pending notice only the first time this kind occurs; seen_ is
    // never reset, capping notices at one per kind for the session's life.
    void notice_once(Failure failure) noexcept {
        const auto index = static_cast<std::size_t>(failure);
        if (index >= kFailureCount) return;
        if (seen_.take(index)) pending_[index] = true;
    }

    Device player_{};
    AudioState state_ = AudioState::unprepared;
    std::array<bool, kFailureCount> pending_{};
    once_flags::Flags<kFailureCount> seen_{};
};

// Per-drain tally. The report is a local value returned by drain(); nothing
// accumulates on the session, so repeated drains cannot overflow shared counts.
struct DeliveryReport {
    std::size_t consumed = 0;
    std::size_t started = 0;
    std::size_t skipped = 0;
    std::size_t failed = 0;
    std::size_t invalid = 0;
};

// Consume every cue currently in the batch, sending each exactly once and
// counting the result. Sink::send must be nonthrowing and return one of the
// declared Delivery values. Batch is bounded (at most four cues per tick times the
// runner's maximum tick batch, i.e. <= 24), and a malformed report is already
// rejected by Batch::from rather than being swallowed here.
template <class Sink>
DeliveryReport drain(Batch& batch, Sink& sink) noexcept {
    DeliveryReport report;
    while (std::optional<Kind> kind = batch.take()) {
        ++report.consumed;
        switch (sink.send(*kind)) {
            case Delivery::started:      ++report.started; break;
            case Delivery::skipped:      ++report.skipped; break;
            case Delivery::failed:       ++report.failed;  break;
            case Delivery::invalid_kind: ++report.invalid; break;
        }
    }
    return report;
}

}  // namespace study_sound
