#include "net/socket.h"

#ifdef _WIN32
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <winsock2.h>
#  include <ws2tcpip.h>
#else
#  include <arpa/inet.h>
#  include <cerrno>
#  include <netinet/in.h>
#  include <sys/socket.h>
#  include <unistd.h>
#endif

namespace study_net {
#ifdef _WIN32
static_assert(sizeof(Runtime::Native) == sizeof(SOCKET));
static_assert(Runtime::kInvalid == INVALID_SOCKET);
#endif
namespace {

#ifdef _WIN32
using native_t = SOCKET;
using socklen_type = int; // Winsock length parameters are plain int
constexpr native_t kNativeInvalid = INVALID_SOCKET;
#else
using native_t = int;
using socklen_type = socklen_t;
constexpr native_t kNativeInvalid = -1;
#endif

int last_error() noexcept {
#ifdef _WIN32
    return WSAGetLastError();
#else
    return errno;
#endif
}

bool is_interrupted(int e) noexcept {
#ifdef _WIN32
    (void)e;
    return false; // Winsock WSAEINTR is cancellation, not a POSIX signal interruption.
#else
    return e == EINTR;
#endif
}

int invalid_argument_error() noexcept {
#ifdef _WIN32
    return WSAEINVAL;
#else
    return EINVAL;
#endif
}

void close_native(native_t n) noexcept {
#ifdef _WIN32
    ::closesocket(n);
#else
    ::close(n);
#endif
}

#ifdef __APPLE__
// macOS has no MSG_NOSIGNAL, so SIGPIPE is suppressed per-socket. Failing to
// install this option is treated as a hard error by the callers.
bool configure_no_sigpipe(native_t fd, int& error) noexcept {
    int one = 1;
    if (::setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &one,
                     static_cast<socklen_type>(sizeof(one))) != 0) {
        error = last_error();
        return false;
    }
    return true;
}
#endif

} // namespace

Runtime::Runtime() noexcept : ready_(false), error_(0) {
#ifdef _WIN32
    WSADATA data;
    const int rc = ::WSAStartup(MAKEWORD(2, 2), &data);
    if (rc != 0) {
        error_ = rc; // ready_ stays false; no matching WSACleanup is owed
        return;
    }
#endif
    ready_ = true; // POSIX needs no startup; consider it ready immediately
}

Runtime::~Runtime() {
#ifdef _WIN32
    if (ready_) {
        ::WSACleanup();
    }
#endif
}

void Socket::reset() noexcept {
    if (Runtime::native_valid(native_)) {
        // Close exactly once. A POSIX close() interrupted by a signal is NOT
        // blindly retried: the descriptor state is ambiguous after EINTR.
        const auto owned = release();
        close_native(static_cast<native_t>(owned));
    }
}

Socket listen_loopback(std::uint16_t requested_port,
                       std::uint16_t& bound_port,
                       int& error) noexcept {
    bound_port = 0;
    error = 0;

    const native_t fd = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (fd == kNativeInvalid) {
        error = last_error();
        return Socket();
    }
    Socket candidate(static_cast<Runtime::Native>(fd));

#ifdef _WIN32
    // Request exclusive binding for this listener. Required before
    // bind(), and a failure here is fatal for this teaching demo.
    BOOL exclusive = TRUE;
    if (::setsockopt(fd, SOL_SOCKET, SO_EXCLUSIVEADDRUSE,
                     reinterpret_cast<const char*>(&exclusive),
                     static_cast<socklen_type>(sizeof(exclusive))) != 0) {
        error = last_error();
        return Socket(); // candidate closes on return, before error is used
    }
#elif defined(__APPLE__)
    if (!configure_no_sigpipe(fd, error)) {
        return Socket();
    }
#endif
    // POSIX/Linux: deliberately no SO_REUSEADDR in this teaching demo.

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(requested_port);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    if (::bind(fd, reinterpret_cast<const sockaddr*>(&addr),
               static_cast<socklen_type>(sizeof(addr))) != 0) {
        error = last_error();
        return Socket();
    }

    if (::listen(fd, 1) != 0) { // pending-connection queue hint, not a total-client limit
        error = last_error();
        return Socket();
    }

    sockaddr_in actual{};
    socklen_type actual_len = static_cast<socklen_type>(sizeof(actual));
    if (::getsockname(fd, reinterpret_cast<sockaddr*>(&actual), &actual_len) != 0) {
        error = last_error();
        return Socket();
    }
    bound_port = ntohs(actual.sin_port);

    return candidate; // NRVO, or move when copy elision is not performed
}

Socket accept_one(const Socket& listener, int& err) noexcept {
    err = 0;
    if (!listener.valid()) {
        err = invalid_argument_error();
        return Socket();
    }

    for (;;) {
        const native_t fd = ::accept(static_cast<native_t>(listener.native()),
                                     nullptr, nullptr);
        if (fd != kNativeInvalid) {
#ifdef __APPLE__
            if (!configure_no_sigpipe(fd, err)) {
                close_native(fd); // do not leak an unconfigured accepted socket
                return Socket();
            }
#endif
            return Socket(static_cast<Runtime::Native>(fd));
        }
        const int e = last_error();
        if (is_interrupted(e)) {
            continue; // accept() is safe to retry after EINTR
        }
        err = e;
        return Socket();
    }
}

Socket connect_loopback(std::uint16_t port, int& error) noexcept {
    error = 0;
    if (port == 0) {
        error = invalid_argument_error();
        return Socket();
    }

    const native_t fd = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (fd == kNativeInvalid) {
        error = last_error();
        return Socket();
    }
    Socket candidate(static_cast<Runtime::Native>(fd));

#ifdef __APPLE__
    if (!configure_no_sigpipe(fd, error)) {
        return Socket();
    }
#endif

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    if (::connect(fd, reinterpret_cast<const sockaddr*>(&addr),
                  static_cast<socklen_type>(sizeof(addr))) != 0) {
        // An interrupted connect is NOT blindly retried: the state of the
        // half-open connection is ambiguous, so report the failure instead.
        error = last_error();
        return Socket();
    }

    return candidate;
}

bool send_byte(Socket& socket, std::uint8_t byte, int& error) noexcept {
    error = 0;
    if (!socket.valid()) {
        error = invalid_argument_error();
        return false;
    }

    for (;;) {
#ifdef MSG_NOSIGNAL
        const int flags = MSG_NOSIGNAL; // Linux: avoids process-wide SIGPIPE
#else
        const int flags = 0; // Windows; macOS uses SO_NOSIGPIPE instead
#endif
        const auto n = ::send(static_cast<native_t>(socket.native()),
                              reinterpret_cast<const char*>(&byte), 1, flags);
        if (n == 1) {
            return true; // handed to the OS; not delivery, not an ACK
        }
        if (n == 0) {
            error = invalid_argument_error(); // no progress for a nonempty request
            return false;
        }
        const int e = last_error();
        if (is_interrupted(e)) {
            continue; // interrupted before transferring this byte
        }
        error = e;
        return false;
    }
}

bool receive_byte(Socket& socket, std::uint8_t& byte, bool& eof,
                  int& error) noexcept {
    error = 0;
    eof = false;
    if (!socket.valid()) {
        error = invalid_argument_error();
        return false;
    }

    for (;;) {
        char buffer = 0;
        const auto n = ::recv(static_cast<native_t>(socket.native()),
                              &buffer, 1, 0);
        if (n == 1) {
            byte = static_cast<std::uint8_t>(
                static_cast<unsigned char>(buffer));
            return true;
        }
        if (n == 0) {
            eof = true; // orderly shutdown by the peer; not an error
            error = 0;
            return false;
        }
        const int e = last_error();
        if (is_interrupted(e)) {
            continue; // safe to retry after EINTR
        }
        error = e;
        return false;
    }
}

} // namespace study_net
