#pragma once

// Portable, header-only classification rules for a Windows IOCP-style
// receiver. This is decision logic only: no Win32 headers are included and
// there is no allocation, I/O, or object-lifecycle implementation.

#include <array>
#include <cstddef>
#include <cstdint>

namespace study_net {

// Outcome of attempting to start an overlapped receive.
enum class Submission {
    immediate,  // call reported immediate success; the operation is complete
    pending,    // call failed with the pending code; a completion will post
    rejected    // call failed otherwise; no completion packet will arrive
};

// Classify a receive call from its return value, the accompanying last-error,
// and the platform pending error code.
//
// A zero result reports immediate completion. Any nonzero result is failure-valued:
// it is pending only when last_error equals pending_code, else rejected.
constexpr Submission classify_submission(int call_result, int last_error,
                                         int pending_code) noexcept {
    return call_result == 0
               ? Submission::immediate
               : (last_error == pending_code ? Submission::pending
                                             : Submission::rejected);
}

// Kind of a posted completion. Meaningful only for a positive-length TCP
// receive.
enum class CompletionKind {
    data,       // success with a positive byte count
    eof,        // success with a zero byte count
    cancelled,  // failure whose error equals the cancellation code
    error       // any other failure
};

// Result of a posted completion. request is an opaque correlation key.
// bytes is a fixed-size inline prefix of the received payload.
struct CompletionResult {
    std::uint64_t request = 0;
    CompletionKind kind = CompletionKind::error;
    std::size_t count = 0;
    std::uint32_t error = 0;
    std::array<std::uint8_t, 16> bytes{};
};

// Classify a posted completion. Success wins: a positive count is data and a
// zero count is eof. On failure, cancelled_code yields cancelled, else error.
//
// Caller notes:
//  - Capacity passed to a receive is a maximum, not a fill target; count is
//    the number of bytes actually transferred.
//  - This receiver does not consume a failed completion as payload; it leaves
//    count at zero and reports the error instead.
//  - A zero-length probe has a different meaning than success-with-eof and is
//    interpreted by the caller, not here.
//  - Immediate submission does not imply the completion packet has already
//    been dequeued; it may still be delivered through the completion queue.
constexpr CompletionKind classify_completion(bool success, std::uint32_t count,
                                             std::uint32_t error,
                                             std::uint32_t cancelled_code) noexcept {
    if (success) {
        return count > 0 ? CompletionKind::data : CompletionKind::eof;
    }
    return error == cancelled_code ? CompletionKind::cancelled
                                   : CompletionKind::error;
}

}  // namespace study_net
