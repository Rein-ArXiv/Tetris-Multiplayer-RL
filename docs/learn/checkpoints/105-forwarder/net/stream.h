#ifndef STUDY_NET_STREAM_H
#define STUDY_NET_STREAM_H

#include <cstddef>
#include <cstdint>
#include <limits>

#include "net/socket.h"  // existing cumulative header: Runtime, Socket, loopback helpers

namespace study_net {

// Result of one low-level socket transfer attempt.
//   progress : count is in [1, requested]; error is 0.
//   eof      : receive observed a clean peer shutdown; count is 0; error is 0.
//   error    : count is 0; error is the platform code (or EINVAL/WSAEINVAL).
enum class StreamStatus { progress, eof, error };

struct StreamResult {
    StreamStatus status;
    std::size_t count;
    int error;
};

// Clamp requests to a portable per-call limit. Winsock send()/recv() take int
// length, so anything above INT_MAX must be split by the caller's loop.
constexpr int io_chunk_size(std::size_t requested) noexcept {
    return requested > static_cast<std::size_t>((std::numeric_limits<int>::max)())
               ? (std::numeric_limits<int>::max)()
               : static_cast<int>(requested);
}

// Send up to `count` readable bytes from the borrowed buffer `data`.
// Rejects an invalid socket, a null buffer or a zero-length request with
// EINVAL / WSAEINVAL. The caller owns `data` and guarantees `count` readable.
StreamResult send_some(Socket& socket, const std::uint8_t* data, std::size_t count) noexcept;

// Receive up to `count` bytes into the borrowed writable buffer `data`.
// A zero-length request is rejected so that a recv() of 0 unambiguously means
// EOF. The caller owns `data` and guarantees `count` writable bytes.
StreamResult receive_some(Socket& socket, std::uint8_t* data, std::size_t count) noexcept;

// Half-close the send direction (SHUT_WR / SD_SEND); the receive direction
// stays available. Returns true on success, otherwise false with `error` set
// to the code captured immediately from the failing call.
bool shutdown_send(Socket& socket, int& error) noexcept;

}  // namespace study_net

#endif  // STUDY_NET_STREAM_H
