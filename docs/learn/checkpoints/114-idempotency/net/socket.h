#ifndef STUDY_NET_SOCKET_H
#define STUDY_NET_SOCKET_H

// study_net: a tiny, teaching-sized blocking IPv4 TCP loopback layer.
//
// Teaching notes
// --------------
// * Runtime vs Socket lifetime: every Socket must be destroyed BEFORE the
//   Runtime that produced it, because sockets are OS resources drawn from a
//   started networking stack. Declare the Runtime first and Sockets after it,
//   so reverse destruction order closes sockets first.
// * Handle vs peer endpoint: a Socket owns an OS handle (a native descriptor).
//   The peer endpoint (address + port) is NOT stored in Socket. Loopback is
//   fixed by this API; the peer of an accepted/connected socket is whatever the
//   OS reports. This exercise never needs to format a peer.
// * Descriptor 0 is a valid handle. Never use 0 as "invalid" on POSIX; use -1
//   (or all-bits-one on Windows). native_valid() encodes that rule.
//
// Supported: Windows (WinSock2), Linux, macOS. This header deliberately avoids
// OS headers: it uses integer-only native types so <winsock2.h> stays in the
// .cpp. No DNS, no framing, no protocol, no game networking.

#include <cstdint>

namespace study_net {

// Owns one successful Winsock startup reference; POSIX needs no startup.
class Runtime {
public:
#ifdef _WIN32
    using Native = std::uintptr_t;                              // SOCKET-sized
    static constexpr Native kInvalid = ~static_cast<Native>(0); // INVALID_SOCKET
#else
    using Native = int;                                         // file descriptor
    static constexpr Native kInvalid = -1; // never 0: 0 is a valid descriptor
#endif

    // True when a native handle has a valid numeric representation; false for the invalid sentinel.
    static constexpr bool native_valid(Native n) noexcept {
        #ifdef _WIN32
        return n != kInvalid;
#else
        return n >= 0;
#endif
    }

    Runtime() noexcept;
    ~Runtime();

    Runtime(const Runtime&) = delete;
    Runtime& operator=(const Runtime&) = delete;
    Runtime(Runtime&&) = delete;
    Runtime& operator=(Runtime&&) = delete;

    // True once the OS networking stack is ready for use.
    bool ready() const noexcept { return ready_; }

    // Non-zero platform error captured while starting (0 on success).
    int error() const noexcept { return error_; }

private:
    bool ready_;
    int error_;
};

// Move-only RAII owner of exactly one native socket handle.
//
// Single-thread ownership is assumed: no internal locking, no shutdown() in the
// destructor, no sharing of handles. Copying is deleted; moving transfers the
// handle and leaves the source invalid, so moves do not duplicate ownership of one
// descriptor and close() is exactly-once.
class Socket {
public:
    Socket() noexcept : native_(Runtime::kInvalid) {}

    // Adopt an already-created native handle. This Socket takes ownership and
    // will close it once; the caller must not close it afterwards.
    explicit Socket(Runtime::Native adopted) noexcept : native_(adopted) {}

    ~Socket() noexcept { reset(); }

    Socket(const Socket&) = delete;
    Socket& operator=(const Socket&) = delete;

    Socket(Socket&& other) noexcept : native_(other.release()) {}

    Socket& operator=(Socket&& other) noexcept {
        if (this != &other) {
            reset();
            native_ = other.release();
        }
        return *this;
    }

    bool valid() const noexcept { return Runtime::native_valid(native_); }

    // Non-owning view of the OS handle. Borrowed until close, destruction, release, or ownership replacement.
    Runtime::Native native() const noexcept { return native_; }

    // Close now and invalidate immediately, so the handle cannot be closed
    // twice. Closing an already-invalid socket is a no-op.
    void reset() noexcept;

    // Give up ownership without closing (used by move).
    Runtime::Native release() noexcept {
        Runtime::Native n = native_;
        native_ = Runtime::kInvalid;
        return n;
    }

private:
    Runtime::Native native_;
};

// Create a listening socket bound to 127.0.0.1. If requested_port is 0 the OS
// picks an ephemeral port, reported back through bound_port. On failure returns
// an invalid Socket and stores the captured platform error in error.
Socket listen_loopback(std::uint16_t requested_port,
                       std::uint16_t& bound_port,
                       int& error) noexcept;

// Blocking accept of exactly one connection. Retries POSIX EINTR only; Winsock cancellation is reported. Returns an
// owned Socket (possibly invalid on failure); stores the error in err.
Socket accept_one(const Socket& listener, int& err) noexcept;

// Blocking connect to 127.0.0.1:port. Port 0 is rejected. Does NOT blindly
// retry an interrupted connect. Returns an invalid Socket on failure.
Socket connect_loopback(std::uint16_t port, int& error) noexcept;

// Send exactly one byte. Retries POSIX EINTR only; Winsock cancellation is reported. Returns true only when the byte
// was handed to the OS: success is NOT proof of delivery and NOT an ACK.
bool send_byte(Socket& socket, std::uint8_t byte, int& error) noexcept;

// Receive exactly one byte. On clean EOF returns false, sets eof=true, error=0.
// Any other failure returns false and captures the platform error.
bool receive_byte(Socket& socket, std::uint8_t& byte, bool& eof,
                  int& error) noexcept;

} // namespace study_net

#endif // STUDY_NET_SOCKET_H
