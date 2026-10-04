#ifndef STUDY_NET_FORWARD_DIRECTION_H
#define STUDY_NET_FORWARD_DIRECTION_H

// One-direction forwarding: a FrameParser-fed byte source -> one destination
// socket. The actual socket lives outside this header; this type only decides
// what to send and when.
//
// Wire format (teaching study, from framing.h):
//   [LEN:u16 little endian][TYPE:u8][PAYLOAD:LEN-1]   (no checksum)
// LEN counts TYPE + PAYLOAD, in [1, 33]. Every accepted application frame is
// forwarded byte-exact: there is no game validation, authentication, or
// filtering here. encode_frame() only re-serialises the already parsed frame,
// so forwarding is deterministic for a known-valid frame; a malformed second
// header is reported only once the first complete frame has been sent.
//
// Responsibilities
//   * FrameParser (owned):  received bytes -> whole frames, keeps an
//                           incomplete tail.
//   * ForwardDirection:     owns the parser, at most one partially sent
//                           EncodedFrame, and the source/send state machine.
//   * Caller:               owns source and destination sockets, performs recv,
//                           and the non-blocking send callback. It must feed
//                           every read before deciding to read again.
//
// Bounds / ordering
//   * The parser buffer is kReceiveCapacity (64) bytes, so a conventional
//     16-byte read leaves headroom for the retained tail (34 + 16 < 64).
//   * A whole frame is always smaller than the receive buffer, so once a frame
//     is primed it no longer depends on the source.
//   * The prefix passed to the constructor is always processed before any
//     recv: the constructor primes the first complete frame (if any).
//   * While a frame is pending, can_read() is false and no further bytes are
//     consumed: fixed-capacity backpressure, no growth, no allocations.
//
// Threading
//   This type is single-owner and has no threads, mutexes, clocks, or socket
//   calls. Copy/move deletion prevents duplicating cursors; the caller must
//   still serialize access through any references.

#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <utility>

#include "net/framing.h"
#include "net/send_budget.h"

namespace study_net {

// Conventional non-blocking read size. The fixed receive capacity, not this
// hint, is what actually bounds memory and rejects overflow.
inline constexpr std::size_t kSuggestedReadBytes = 16;

// Lifecycle of a forwarding direction. `running` is the only non-terminal
// state. Once terminal, a direction never becomes usable again; `stop()` and
// `eof()` do not resurrect it.
enum class ForwardState {
    running,
    protocol_error,       // malformed wire (bad length / parser failure)
    buffer_limit,         // source bytes would overflow the parser buffer
    send_error,           // destination reported an error
    invalid_send_result,  // send callback returned an impossible attempt
    source_closed,        // clean EOF with no partial frame and nothing pending
    truncated,            // EOF with an incomplete frame tail in the parser
    undelivered,          // EOF while a complete frame was still unsent
    cancelled             // cooperative stop() before completion
};

// Forwards at most one frame at a time from a FrameParser to a destination.
class ForwardDirection {
public:
    ForwardDirection() noexcept { parse_buffered(); }

    // Adopt `prefix` and reset the source parser. Any complete frame already
    // buffered is primed immediately; nothing is sent by the constructor.
    ForwardDirection(FrameParser&& prefix) noexcept {
        parser_ = std::move(prefix);
        prefix = FrameParser{};
        parse_buffered();
    }

    ForwardDirection(const ForwardDirection&) = delete;
    ForwardDirection& operator=(const ForwardDirection&) = delete;
    ForwardDirection(ForwardDirection&&) = delete;
    ForwardDirection& operator=(ForwardDirection&&) = delete;
    ~ForwardDirection() noexcept = default;

    // True when the source may be read again: running and no frame is pending.
    // False also covers every terminal state.
    bool can_read() const noexcept {
        return state_ == ForwardState::running && !has_pending();
    }

    // True while an encoded frame is owned but not fully accepted by the sink.
    bool has_pending() const noexcept { return pending_.size > offset_; }

    // Bytes of the current frame not yet accepted by the sink (0 when none).
    std::size_t bytes_pending() const noexcept { return pending_.size - offset_; }

    // Unconsumed bytes still inside the parser (an incomplete tail once drained).
    std::size_t buffered_bytes() const noexcept { return parser_.pending_bytes(); }

    ForwardState state() const noexcept { return state_; }

    // Error code reported by the destination; zero may mean no specific code.
    int last_error() const noexcept { return last_error_; }

    // Accept source bytes. Returns true iff the bytes were appended to the
    // parser. Check can_read() BEFORE recv: false after an overflow is terminal,
    // and false while pending does not retain new bytes for a later retry.
    // Positive count requires a valid data pointer. A protocol error while priming returns
    // true (the bytes were consumed); inspect state().
    bool feed(const std::uint8_t* data, std::size_t count) noexcept {
        if (!can_read()) {
            return false;
        }
        if (count > 0 && data == nullptr) {
            return false;
        }
        if (!parser_.append(data, count)) {
            state_ = ForwardState::buffer_limit;
            return false;
        }
        parse_buffered();
        return true;
    }

    // One non-blocking destination attempt, never a loop. Sends only when
    // running with a pending frame. Returns the (possibly unchanged) state.
    // Sender must return promptly and must not reenter or mutate this object.
    // Acceptance means local send progress, not peer application receipt.
    template <typename Sender>
    ForwardState flush(Sender&& send) noexcept {
        static_assert(
            std::is_nothrow_invocable_r_v<SendAttempt, Sender&,
                                          const std::uint8_t*, std::size_t>,
            "Sender must be noexcept and return SendAttempt");

        if (state_ != ForwardState::running || !has_pending()) {
            return state_;
        }

        const std::size_t remaining = pending_.size - offset_;
        const SendAttempt attempt =
            send(pending_.bytes.data() + offset_, remaining);

        switch (attempt.state) {
            case SendState::progress:
                if (attempt.count < 1 || attempt.count > remaining ||
                    attempt.error != 0) {
                    state_ = ForwardState::invalid_send_result;
                    return state_;
                }
                offset_ += attempt.count;
                if (offset_ == pending_.size) {
                    // Whole frame accepted: drop it and prime the next buffered
                    // frame, but never send it from this call.
                    pending_ = EncodedFrame{};
                    offset_ = 0;
                    parse_buffered();
                }
                return state_;

            case SendState::would_block:
            case SendState::interrupted:
                // Retain the unsent bytes; count and error must both be zero.
                if (attempt.count != 0 || attempt.error != 0) {
                    state_ = ForwardState::invalid_send_result;
                }
                return state_;

            case SendState::error:
                if (attempt.count != 0) {
                    state_ = ForwardState::invalid_send_result;
                    return state_;
                }
                // Already locally accepted bytes cannot be recalled; the rest is
                // aborted. No automatic retry or reconnect.
                last_error_ = attempt.error;
                state_ = ForwardState::send_error;
                return state_;

            default:
                state_ = ForwardState::invalid_send_result;
                return state_;
        }
    }

    // Source EOF. Immediate-abort policy for this game, not generic TCP
    // half-close draining: a complete but unsent frame is `undelivered`,
    // while only an incomplete parser tail is `truncated`. A clean close is
    // `source_closed`. Callers normally never see EOF while pending because
    // can_read() is false then, but the source may close independently.
    ForwardState eof() noexcept {
        if (state_ != ForwardState::running) {
            return state_;
        }
        if (has_pending()) {
            state_ = ForwardState::undelivered;
        } else if (parser_.pending_bytes() > 0) {
            state_ = ForwardState::truncated;
        } else {
            state_ = ForwardState::source_closed;
        }
        return state_;
    }

    // Cooperative cancellation: running -> cancelled only. Diagnostic bytes
    // may remain owned until the object is destroyed; there is no resurrection.
    ForwardState stop() noexcept {
        if (state_ == ForwardState::running) {
            state_ = ForwardState::cancelled;
        }
        return state_;
    }

private:
    // Try to move exactly one complete frame from the parser into pending_.
    // need_more keeps running; malformed wire latches protocol_error. Never
    // touched while a frame is already pending.
    void parse_buffered() noexcept {
        if (state_ != ForwardState::running || has_pending()) {
            return;
        }
        Frame frame{};
        const ParseStatus status = parser_.next(frame);
        if (status == ParseStatus::need_more) {
            return;
        }
        if (status == ParseStatus::error) {
            state_ = ForwardState::protocol_error;
            return;
        }
        EncodedFrame encoded{};
        if (!encode_frame(frame, encoded)) {
            // Only reachable if the parser ever produced an over-long frame;
            // treated as a wire fault because encode itself cannot fail here.
            state_ = ForwardState::protocol_error;
            return;
        }
        pending_ = encoded;
        offset_ = 0;
    }

    FrameParser parser_{};
    EncodedFrame pending_{};
    std::size_t offset_{0};
    int last_error_{0};
    ForwardState state_{ForwardState::running};
};

}  // namespace study_net

#endif  // STUDY_NET_FORWARD_DIRECTION_H
