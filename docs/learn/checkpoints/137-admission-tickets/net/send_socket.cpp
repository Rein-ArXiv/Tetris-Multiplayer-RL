// Platform socket headers first: on Windows winsock2.h must precede any header
// that could pull in windows.h.
#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>
#include <fcntl.h>
#endif

#include "net/send_socket.h"
#include "net/stream.h"

#include <algorithm>
#include <chrono>
#include <climits>
#include <thread>

namespace study_net {
namespace {

#if defined(_WIN32)
constexpr int kInvalidArgument = WSAEINVAL;
#else
constexpr int kInvalidArgument = EINVAL;
#endif

}  // namespace

bool set_nonblocking(Socket& socket, bool enabled, int& error) noexcept {
    if (!socket.valid()) {
        error = kInvalidArgument;
        return false;
    }

#if defined(_WIN32)
    u_long mode = enabled ? 1UL : 0UL;
    if (::ioctlsocket(static_cast<SOCKET>(socket.native()), FIONBIO, &mode) == SOCKET_ERROR) {
        error = ::WSAGetLastError();
        return false;
    }
#else
    const int fd = static_cast<int>(socket.native());
    const int flags = ::fcntl(fd, F_GETFL, 0);
    if (flags < 0) {
        error = errno;
        return false;
    }
    // Preserve unrelated flags; only toggle O_NONBLOCK.
    const int updated = enabled ? (flags | O_NONBLOCK) : (flags & ~O_NONBLOCK);
    if (::fcntl(fd, F_SETFL, updated) < 0) {
        // Capture immediately; a failed fcntl is not retried on EINTR.
        error = errno;
        return false;
    }
#endif

    error = 0;
    return true;
}

SendAttempt try_send(Socket& socket, const std::uint8_t* data, std::size_t size) noexcept {
    if (!socket.valid()) {
        return SendAttempt{SendState::error, 0, kInvalidArgument};
    }
    if (data == nullptr || size == 0) {
        return SendAttempt{SendState::error, 0, kInvalidArgument};
    }

#if defined(_WIN32)
    const SOCKET handle = static_cast<SOCKET>(socket.native());
    const int chunk = io_chunk_size(size);
    const int n = ::send(handle, reinterpret_cast<const char*>(data), chunk, 0);
    if (n == SOCKET_ERROR) {
        const int err = ::WSAGetLastError();
        if (err == WSAEWOULDBLOCK) {
            return SendAttempt{SendState::would_block, 0, 0};
        }
        // WSAEINTR is cancellation here, not a retryable interruption.
        return SendAttempt{SendState::error, 0, err};
    }
    if (n > 0) {
        return SendAttempt{SendState::progress, static_cast<std::size_t>(n), 0};
    }
    // A zero-byte send is a synthetic failure; do not read stale error state.
    return SendAttempt{SendState::error, 0, WSAECONNABORTED};
#else
#if defined(__linux__)
    constexpr int kSendFlags = MSG_NOSIGNAL;
#else
    constexpr int kSendFlags = 0;  // macOS: SIGPIPE already suppressed via SO_NOSIGPIPE.
#endif
    const int fd = static_cast<int>(socket.native());
    const int chunk = io_chunk_size(size);
    const ssize_t n = ::send(fd, data, static_cast<std::size_t>(chunk), kSendFlags);
    if (n < 0) {
        const int err = errno;
        if (err == EINTR) {
            return SendAttempt{SendState::interrupted, 0, 0};
        }
        if (err == EAGAIN || err == EWOULDBLOCK) {
            return SendAttempt{SendState::would_block, 0, 0};
        }
        return SendAttempt{SendState::error, 0, err};
    }
    if (n > 0) {
        return SendAttempt{SendState::progress, static_cast<std::size_t>(n), 0};
    }
    return SendAttempt{SendState::error, 0, EIO};
#endif
}

SendReport send_bounded(Socket& socket,
                        const std::uint8_t* data,
                        std::size_t size,
                        std::chrono::steady_clock::time_point deadline,
                        const std::atomic_bool* cancel) {
    // Requires a nonblocking socket; the mode is not changed here.
    const auto sender = [&socket](const std::uint8_t* bytes, std::size_t count) {
        return try_send(socket, bytes, count);
    };
    const auto now = []() { return std::chrono::steady_clock::now(); };
    const auto pause = [](std::chrono::steady_clock::duration wait) {
        std::this_thread::sleep_for(wait);
    };
    const auto cancelled = [cancel]() {
        return cancel != nullptr && cancel->load(std::memory_order_relaxed);
    };
    return send_until(data, size, deadline, sender, now, pause, cancelled);
}

}  // namespace study_net
