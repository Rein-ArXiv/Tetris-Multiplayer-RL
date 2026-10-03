#if defined(_WIN32)
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <winsock2.h>
#else
#  include <poll.h>
#  include <cerrno>
#endif

#include "net/read_hint.h"

namespace study_net {

ReadHint wait_readable(const Socket& socket, int timeout_ms) noexcept {
    ReadHint hint{};

    if (!socket.valid() || timeout_ms < 0 || timeout_ms > 1000) {
        hint.state = WaitState::error;
#if defined(_WIN32)
        hint.error = WSAEINVAL;
#else
        hint.error = EINVAL;
#endif
        return hint;
    }

#if defined(_WIN32)
    WSAPOLLFD pfd{};
    pfd.fd = static_cast<SOCKET>(socket.native());
    pfd.events = POLLRDNORM;
    const int rc = ::WSAPoll(&pfd, 1, timeout_ms);
#else
    struct pollfd pfd{};
    pfd.fd = socket.native();
    pfd.events = POLLIN;
    const int rc = ::poll(&pfd, 1, timeout_ms);
#endif

    if (rc == 0) {
        hint.state = WaitState::timeout;
        return hint;
    }

    if (rc < 0) {
#if defined(_WIN32)
        hint.error = ::WSAGetLastError();
        hint.state = WaitState::error;
#else
        const int err = errno;
        hint.error = err;
        hint.state = (err == EINTR) ? WaitState::interrupted : WaitState::error;
#endif
        return hint;
    }

    const short revents = pfd.revents;

    if (revents & POLLNVAL) {
        hint.state = WaitState::error;
#if defined(_WIN32)
        hint.error = WSAENOTSOCK;
#else
        hint.error = EBADF;
#endif
        return hint;
    }

#if defined(_WIN32)
    hint.readable = (revents & POLLRDNORM) != 0;
#else
    hint.readable = (revents & POLLIN) != 0;
#endif
    hint.hangup = (revents & POLLHUP) != 0;
    hint.fault = (revents & POLLERR) != 0;

    if (hint.readable || hint.hangup || hint.fault) {
        hint.state = WaitState::ready;
        hint.error = 0;
        return hint;
    }

    hint.state = WaitState::error;
#if defined(_WIN32)
    hint.error = WSAEINVAL;
#else
    hint.error = EIO;
#endif
    return hint;
}

} // namespace study_net
