#ifndef STUDY_NET_SEND_SOCKET_H
#define STUDY_NET_SEND_SOCKET_H

// Bounded nonblocking send helpers over the existing study_net Socket.

#include "net/send_budget.h"
#include "net/socket.h"

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>

namespace study_net {

// Toggle nonblocking mode in both directions. The caller must have exclusive
// access and restores blocking mode explicitly when needed. On failure returns
// false and stores the immediate error; error is 0 on success.
bool set_nonblocking(Socket& socket, bool enabled, int& error) noexcept;

// One OS send attempt on a nonblocking socket. There is no internal EINTR retry,
// so the outer loop can recheck its deadline and cancellation. An invalid socket,
// a zero size or a positive-null request yields an error attempt.
SendAttempt try_send(Socket& socket, const std::uint8_t* data, std::size_t size) noexcept;

// Bounded send using the real clock, sleep_for and an optional cancel flag. The
// socket must already be nonblocking; the mode is not changed here.
SendReport send_bounded(Socket& socket,
                        const std::uint8_t* data,
                        std::size_t size,
                        std::chrono::steady_clock::time_point deadline,
                        const std::atomic_bool* cancel = nullptr);

}  // namespace study_net

#endif  // STUDY_NET_SEND_SOCKET_H
