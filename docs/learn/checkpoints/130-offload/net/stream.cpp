#include "net/stream.h"

#include <cerrno>

#if defined(_WIN32)
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <winsock2.h>
#  include <ws2tcpip.h>
#else
#  include <sys/socket.h>
#  include <sys/types.h>
#  include <unistd.h>
#endif

namespace study_net {
namespace {

#if defined(_WIN32)
constexpr int kInvalidRequest = WSAEINVAL;
constexpr int kZeroSendFailure = WSAECONNABORTED;  // synthetic: send() returned 0
using OsHandle = SOCKET;
#else
constexpr int kInvalidRequest = EINVAL;
constexpr int kZeroSendFailure = EIO;  // synthetic: send() returned 0
constexpr int kInterrupted = EINTR;
using OsHandle = int;
#endif

#if !defined(_WIN32)
// macOS sockets already set SO_NOSIGPIPE, so their send() uses 0 flags; Linux
// has no SO_NOSIGPIPE and must use MSG_NOSIGNAL. No global signal state is
// ever changed.
#  if defined(__APPLE__)
constexpr int kSendFlags = 0;
#  else
constexpr int kSendFlags = MSG_NOSIGNAL;
#  endif
#endif

OsHandle handle_of(Socket& socket) noexcept {
    return static_cast<OsHandle>(socket.native());
}

}  // namespace

StreamResult send_some(Socket& socket, const std::uint8_t* data, std::size_t count) noexcept {
    if (!socket.valid()) {
        return StreamResult{StreamStatus::error, 0, kInvalidRequest};
    }
    if (count == 0 || data == nullptr) {
        return StreamResult{StreamStatus::error, 0, kInvalidRequest};
    }

    const int request = io_chunk_size(count);
    const OsHandle handle = handle_of(socket);

    for (;;) {
#if defined(_WIN32)
        const int sent = ::send(handle,
                                reinterpret_cast<const char*>(data),
                                request,
                                0);
        if (sent == SOCKET_ERROR) {
            const int code = ::WSAGetLastError();
            // Winsock cancellation is reported to the caller.
            return StreamResult{StreamStatus::error, 0, code};
        }
#else
        const ssize_t sent = ::send(handle,
                                    data,
                                    static_cast<std::size_t>(request),
                                    kSendFlags);
        if (sent < 0) {
            const int code = errno;
            if (code == kInterrupted) {
                continue;  // interrupted before any byte was accepted
            }
            return StreamResult{StreamStatus::error, 0, code};
        }
#endif
        if (sent == 0) {
            // send() reporting 0 never means progress and its errno is not fresh.
            return StreamResult{StreamStatus::error, 0, kZeroSendFailure};
        }
        return StreamResult{StreamStatus::progress, static_cast<std::size_t>(sent), 0};
    }
}

StreamResult receive_some(Socket& socket, std::uint8_t* data, std::size_t count) noexcept {
    if (!socket.valid()) {
        return StreamResult{StreamStatus::error, 0, kInvalidRequest};
    }
    if (count == 0 || data == nullptr) {
        return StreamResult{StreamStatus::error, 0, kInvalidRequest};
    }

    const int request = io_chunk_size(count);
    const OsHandle handle = handle_of(socket);

    for (;;) {
#if defined(_WIN32)
        const int received = ::recv(handle,
                                    reinterpret_cast<char*>(data),
                                    request,
                                    0);
        if (received == SOCKET_ERROR) {
            const int code = ::WSAGetLastError();
            // Winsock cancellation is reported to the caller.
            return StreamResult{StreamStatus::error, 0, code};
        }
#else
        const ssize_t received = ::recv(handle,
                                        data,
                                        static_cast<std::size_t>(request),
                                        0);  // blocking; no MSG_WAITALL, no nonblocking
        if (received < 0) {
            const int code = errno;
            if (code == kInterrupted) {
                continue;  // interrupted before any byte was delivered
            }
            return StreamResult{StreamStatus::error, 0, code};
        }
#endif
        if (received == 0) {
            return StreamResult{StreamStatus::eof, 0, 0};
        }
        return StreamResult{StreamStatus::progress, static_cast<std::size_t>(received), 0};
    }
}

bool shutdown_send(Socket& socket, int& error) noexcept {
    error = 0;
    if (!socket.valid()) {
        error = kInvalidRequest;
        return false;
    }

    const OsHandle handle = handle_of(socket);
#if defined(_WIN32)
    if (::shutdown(handle, SD_SEND) == SOCKET_ERROR) {
        error = ::WSAGetLastError();  // captured immediately; no retry policy
        return false;
    }
#else
    if (::shutdown(handle, SHUT_WR) != 0) {
        error = errno;  // captured immediately; no retry policy
        return false;
    }
#endif
    return true;
}

}  // namespace study_net
