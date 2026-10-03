#include "net/socket.h"

#include <charconv>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string_view>
#include <system_error>

namespace {

void print_help(const char* program) {
    std::printf(
        "usage: %s listen PORT | connect PORT\n"
        "\n"
        "Small blocking IPv4 TCP loopback demo run as two processes.\n"
        "  listen PORT   bind 127.0.0.1:PORT (0 picks an ephemeral port),\n"
        "                accept one client, echo one byte, then exit.\n"
        "  connect PORT  connect to 127.0.0.1:PORT, send one byte 42,\n"
        "                read the echoed byte, then exit.\n"
        "\n"
        "Notes:\n"
        "  * Blocking I/O: calls may wait indefinitely; there are no timeouts.\n"
        "  * Loopback only: 127.0.0.1, never a remote or public interface.\n"
        "  * No authentication: any local process able to reach the port may\n"
        "    connect; this is a teaching demo, not a secure service.\n"
        "  * A successful send() only means the bytes were handed to the OS.\n"
        "    It is not proof of delivery and not an acknowledgement (ACK).\n",
        program);
}

// Exact decimal parse of an integer in [0, 65535]; rejects signs, spaces,
// trailing junk, overflow and empty input.
bool parse_port(std::string_view text, std::uint16_t& port_out) {
    if (text.empty()) {
        return false;
    }
    unsigned value = 0;
    const char* first = text.data();
    const char* last = text.data() + text.size();
    const auto result = std::from_chars(first, last, value, 10);
    if (result.ec != std::errc() || result.ptr != last) {
        return false;
    }
    if (value > 65535u) {
        return false;
    }
    port_out = static_cast<std::uint16_t>(value);
    return true;
}

int fail(const char* stage, int error, bool eof) {
    std::fprintf(stderr, "FAIL %s error=%d eof=%s\n", stage, error,
                 eof ? "true" : "false");
    return 1;
}

int run_server(std::uint16_t requested_port) {
    // Runtime is constructed BEFORE any Socket and destroyed AFTER them, so
    // sockets close while the OS networking stack is still available.
    study_net::Runtime runtime;
    if (!runtime.ready()) {
        return fail("runtime", runtime.error(), false);
    }

    std::uint16_t bound_port = 0;
    int error = 0;
    study_net::Socket listener =
        study_net::listen_loopback(requested_port, bound_port, error);
    if (!listener.valid()) {
        return fail("listen", error, false);
    }

    // bound_port is the actual port the OS assigned (possibly ephemeral).
    std::printf("LISTEN 127.0.0.1:%u\n", static_cast<unsigned>(bound_port));
    std::fflush(stdout);

    study_net::Socket peer = study_net::accept_one(listener, error);
    if (!peer.valid()) {
        return fail("accept", error, false);
    }

    // We serve exactly one client: close the listener now, before further I/O,
    // so the port stops accepting and the handle is released promptly.
    listener.reset();

    std::uint8_t byte = 0;
    bool eof = false;
    if (!study_net::receive_byte(peer, byte, eof, error)) {
        return fail("receive", error, eof);
    }

    int send_error = 0;
    if (!study_net::send_byte(peer, byte, send_error)) {
        return fail("send", send_error, false);
    }

    std::printf("RECEIVED %u ECHOED %u\n", static_cast<unsigned>(byte),
                static_cast<unsigned>(byte));
    std::fflush(stdout);
    return 0;
}

int run_client(std::uint16_t port) {
    // Runtime first, Socket second: sockets must die before the runtime.
    study_net::Runtime runtime;
    if (!runtime.ready()) {
        return fail("runtime", runtime.error(), false);
    }

    int error = 0;
    study_net::Socket peer = study_net::connect_loopback(port, error);
    if (!peer.valid()) {
        return fail("connect", error, false);
    }

    const std::uint8_t request = 42;
    int send_error = 0;
    if (!study_net::send_byte(peer, request, send_error)) {
        return fail("send", send_error, false);
    }

    std::uint8_t reply = 0;
    bool eof = false;
    int recv_error = 0;
    if (!study_net::receive_byte(peer, reply, eof, recv_error)) {
        return fail("receive", recv_error, eof);
    }

    if (reply != request) {
        std::fprintf(stderr,
                     "FAIL verify error=0 eof=false expected=%u got=%u\n",
                     static_cast<unsigned>(request), static_cast<unsigned>(reply));
        return 1;
    }

    std::printf("SENT %u RECEIVED %u\n", static_cast<unsigned>(request),
                static_cast<unsigned>(reply));
    std::fflush(stdout);
    return 0;
}

} // namespace

int main(int argc, char** argv) {
    if (argc == 2 && (std::strcmp(argv[1], "--help") == 0 ||
                      std::strcmp(argv[1], "-h") == 0)) {
        print_help(argv[0]);
        return 0;
    }

    if (argc != 3) {
        print_help(argv[0]);
        return 2;
    }

    const std::string_view mode(argv[1]);
    std::uint16_t port = 0;
    if (!parse_port(argv[2], port)) {
        std::fprintf(stderr, "invalid port: %s\n", argv[2]);
        return 2;
    }

    if (mode == "listen") {
        return run_server(port); // port 0 is allowed and picks ephemeral
    }

    if (mode == "connect") {
        if (port == 0) {
            std::fprintf(stderr,
                         "invalid port: connect requires a non-zero port\n");
            return 2;
        }
        return run_client(port);
    }

    print_help(argv[0]);
    return 2;
}
