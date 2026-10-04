#ifndef STUDY_NET_RECEIVE_SOCKET_H
#define STUDY_NET_RECEIVE_SOCKET_H

// One-shot, non-blocking receive helper.
//
// `try_receive` performs exactly one recv() call. It never sleeps, never
// retries and never loops, so a caller that wants a full read must drive it
// itself and be prepared to deal with a partial result. The socket must
// already be switched to non-blocking mode, and the caller must hold exclusive
// access to it: the function takes the descriptor but does not lock or
// serialize anything, and it does not change socket ownership, options or
// blocking mode.

#include <cstddef>
#include <cstdint>

#include "net/socket.h"

namespace study_net {

// Meaning of a single receive attempt.
//   progress    : count is in [1, requested]; error is 0.
//   would_block : the non-blocking socket has no data right now; error is the
//                 platform's would-block code.
//   interrupted : POSIX only: the call was interrupted by a signal (EINTR);
//                 error is EINTR.
//   eof         : the peer performed an orderly shutdown; count is 0,
//                 error is 0.
//   error       : any other failure (including a rejected request or invalid
//                 socket); count is 0 and error is the captured code.
enum class ReceiveState { progress, would_block, interrupted, eof, error };

struct ReceiveAttempt {
  ReceiveState state;
  std::size_t count;
  int error;
};

// Receive up to `count` bytes into the borrowed buffer `data`.
//
// An invalid socket, a null buffer or a zero-length request is rejected with
// EINVAL / WSAEINVAL; the zero-length rule keeps recv()==0 meaning EOF alone.
// A request larger than the portable per-call limit is capped via
// io_chunk_size(). On Windows, WSAEWOULDBLOCK maps to would_block while
// WSAEINTR is reported as error, because Winsock uses it for cancellation
// rather than a retryable signal interruption.
//
// The platform error is captured immediately after the failing call. `data` is
// borrowed for the duration of the call only.
ReceiveAttempt try_receive(Socket& socket, std::uint8_t* data,
                           std::size_t count) noexcept;

}  // namespace study_net

#endif  // STUDY_NET_RECEIVE_SOCKET_H
