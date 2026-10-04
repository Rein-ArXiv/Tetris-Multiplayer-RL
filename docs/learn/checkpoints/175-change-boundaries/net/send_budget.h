#ifndef STUDY_NET_SEND_BUDGET_H
#define STUDY_NET_SEND_BUDGET_H

// Cooperative bounded send driver. The caller owns data and keeps it alive for
// the whole (synchronous) call; this helper only borrows the buffer.

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>

namespace study_net {

enum class SendState {
    progress,
    would_block,
    interrupted,
    error,
};

struct SendAttempt {
    SendState state;
    std::size_t count;
    int error;
};

enum class SendOutcome {
    complete,
    timed_out,
    cancelled,
    error,
    invalid_result,
    invalid_request,
};

struct SendReport {
    SendOutcome outcome;
    std::size_t accepted;
    int error;
};

// Callbacks:
//   Sender   : SendAttempt(const std::uint8_t* data, std::size_t size)
//              one nonblocking attempt; must return promptly.
//   Now      : std::chrono::steady_clock::time_point()  monotonic clock read.
//   Pause    : void(std::chrono::steady_clock::duration) bounded best-effort wait.
//   Cancelled: bool()  cooperative cancellation flag.
// The deadline is cooperative, not hard realtime. On partial failure the already
// accepted prefix is kept; there is no automatic reconnect or retry.
template <typename Sender, typename Now, typename Pause, typename Cancelled>
SendReport send_until(const std::uint8_t* data,
                      std::size_t size,
                      std::chrono::steady_clock::time_point deadline,
                      Sender send,
                      Now now,
                      Pause pause,
                      Cancelled cancelled) {
    // Empty transfer: no callbacks, no pointer arithmetic, null data is fine.
    if (size == 0) {
        return SendReport{SendOutcome::complete, 0, 0};
    }
    if (data == nullptr) {
        return SendReport{SendOutcome::invalid_request, 0, 0};
    }

    std::size_t accepted = 0;
    while (accepted < size) {
        if (cancelled()) {
            return SendReport{SendOutcome::cancelled, accepted, 0};
        }

        const std::chrono::steady_clock::time_point current = now();
        if (current >= deadline) {
            return SendReport{SendOutcome::timed_out, accepted, 0};
        }

        const std::size_t remaining = size - accepted;
        const SendAttempt attempt = send(data + accepted, remaining);

        // Validate before trusting count: progress must move 1..remaining with no
        // error; every other state must report count 0.
        if (attempt.state == SendState::progress) {
            if (attempt.count < 1 || attempt.count > remaining || attempt.error != 0) {
                return SendReport{SendOutcome::invalid_result, accepted, 0};
            }
        } else {
            if (attempt.count != 0) {
                return SendReport{SendOutcome::invalid_result, accepted, 0};
            }
            if (attempt.state != SendState::error && attempt.error != 0) {
                return SendReport{SendOutcome::invalid_result, accepted, 0};
            }
        }

        switch (attempt.state) {
            case SendState::progress:
                accepted += attempt.count;
                break;
            case SendState::would_block: {
                // Same clock type as the deadline keeps (std::min) well-typed;
                // the parentheses dodge any min macro.
                const auto cap = std::chrono::duration_cast<std::chrono::steady_clock::duration>(
                    std::chrono::milliseconds(1));
                const auto after = now();
                if (after >= deadline) return {SendOutcome::timed_out, accepted, 0};
                const auto wait = (std::min)(deadline - after, cap);
                pause(wait);
                break;
            }
            case SendState::interrupted:
                // No sleep: the next iteration rechecks deadline and cancellation.
                break;
            case SendState::error:
                return SendReport{SendOutcome::error, accepted, attempt.error};
            default:
                return SendReport{SendOutcome::invalid_result, accepted, 0};
        }
    }

    // All bytes accepted; crossing the deadline after the final attempt is fine.
    return SendReport{SendOutcome::complete, accepted, 0};
}

}  // namespace study_net

#endif  // STUDY_NET_SEND_BUDGET_H
