#include "net/byte_buffer.h"
#include "net/socket.h"
#include "net/stream.h"

#include <charconv>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <ostream>
#include <string_view>
#include <system_error>

namespace {

enum class Mode { one, two, bytes };

void print_hex(const std::uint8_t* data, std::size_t count) {
    static const char kHexDigits[] = "0123456789ABCDEF";
    for (std::size_t index = 0; index < count; ++index) {
        if (index != 0) {
            std::cout << ' ';
        }
        const unsigned int byte = data[index];
        std::cout << kHexDigits[(byte >> 4) & 0x0Fu] << kHexDigits[byte & 0x0Fu];
    }
}

void print_usage(std::ostream& out) {
    out << "stream_probe: blocking IPv4 loopback-only max-64-byte teaching demo\n"
        << "Usage:\n"
        << "  stream_probe listen PORT READ_CAP\n"
        << "  stream_probe connect PORT MODE\n"
        << "  stream_probe --help\n"
        << "\n"
        << "  PORT      0..65535 (0 selects an ephemeral port; listen only)\n"
        << "  READ_CAP  1..16 bytes requested from each receive call\n"
        << "  MODE      one | two | bytes\n"
        << "\n"
        << "Both endpoints are IPv4 loopback only. There is no authentication,\n"
        << "no framing header and no timeout.\n";
}

int cli_error(const char* reason) {
    std::cerr << "ERROR stage=cli reason=" << reason << "\n";
    print_usage(std::cerr);
    return 2;
}

bool parse_port(const char* text, bool allow_zero, std::uint16_t& value) {
    const std::string_view view(text != nullptr ? text : "");
    if (view.empty()) {
        return false;
    }
    unsigned int parsed = 0;
    const char* first = view.data();
    const char* last = first + view.size();
    const std::from_chars_result result = std::from_chars(first, last, parsed, 10);
    if (result.ec != std::errc() || result.ptr != last) {
        return false;  // not a strict decimal integer
    }
    if (parsed > 65535u) {
        return false;
    }
    if (!allow_zero && parsed == 0u) {
        return false;  // client must dial a concrete port
    }
    value = static_cast<std::uint16_t>(parsed);
    return true;
}

bool parse_read_cap(const char* text, unsigned int& value) {
    const std::string_view view(text != nullptr ? text : "");
    if (view.empty()) {
        return false;
    }
    unsigned int parsed = 0;
    const char* first = view.data();
    const char* last = first + view.size();
    const std::from_chars_result result = std::from_chars(first, last, parsed, 10);
    if (result.ec != std::errc() || result.ptr != last) {
        return false;
    }
    if (parsed < 1u || parsed > 16u) {
        return false;
    }
    value = parsed;
    return true;
}

bool parse_mode(const char* text, Mode& value) {
    const std::string_view view(text != nullptr ? text : "");
    if (view == "one") {
        value = Mode::one;
        return true;
    }
    if (view == "two") {
        value = Mode::two;
        return true;
    }
    if (view == "bytes") {
        value = Mode::bytes;
        return true;
    }
    return false;
}

struct SendOutcome {
    bool ok;
    int error;
    std::size_t accepted;
};

// Each log records one wrapper result; EINTR may cause internal retries.
// TCP may accept fewer
// bytes than requested, so the loop advances by each positive progress count.
// These counts are calls, not segments or messages; the byte stream has no
// boundaries the receiver can rely on.
SendOutcome send_block(study_net::Socket& socket,
                       const std::uint8_t* data,
                       std::size_t count, std::size_t base = 0) {
    std::size_t accepted = 0;
    while (accepted < count) {
        const std::size_t requested = count - accepted;
        const study_net::StreamResult result =
            study_net::send_some(socket, data + accepted, requested);
        if (result.status == study_net::StreamStatus::progress) {
            accepted += result.count;
            std::cout << "SEND requested=" << requested
                      << " accepted=" << result.count
                      << " total=" << base + accepted << "\n";
        } else {
            const int code =
                result.status == study_net::StreamStatus::error ? result.error : 0;
            std::cout << "SEND requested=" << requested
                      << " accepted=0 status="
                      << (result.status == study_net::StreamStatus::eof ? "eof" : "error")
                      << " error=" << code << "\n";
            return SendOutcome{false, code, accepted};
        }
    }
    return SendOutcome{true, 0, accepted};
}

int run_server(std::uint16_t port, unsigned int read_cap) {
    int error = 0;
    std::uint16_t actual = 0;
    study_net::Socket listener = study_net::listen_loopback(port, actual, error);
    if (!listener.valid()) {
        std::cerr << "ERROR stage=listen error=" << error << "\n";
        return 1;
    }

    // Flush before accept so a harness can parse the bound port immediately.
    std::cout << "LISTEN 127.0.0.1:" << actual << std::endl;

    study_net::Socket peer = study_net::accept_one(listener, error);
    if (!peer.valid()) {
        std::cerr << "ERROR stage=accept error=" << error << "\n";
        return 1;
    }
    listener.reset();  // listener is no longer needed

    std::uint8_t scratch[16];
    study_net::ByteBuffer<64> accumulated;
    std::size_t total = 0;

    for (;;) {
        const study_net::StreamResult result =
            study_net::receive_some(peer, scratch, read_cap);
        if (result.status == study_net::StreamStatus::eof) {
            std::cout << "READ eof offset=" << total << "\n";
            break;
        }
        if (result.status == study_net::StreamStatus::error) {
            std::cerr << "ERROR stage=receive error=" << result.error << "\n";
            return 1;
        }
        std::cout << "READ offset=" << total
                  << " count=" << result.count << " hex=";
        print_hex(scratch, result.count);
        std::cout << "\n";
        if (!accumulated.append(scratch, result.count)) {
            std::cerr << "ERROR stage=accumulate reason=overflow capacity=64\n";
            return 1;
        }
        total += result.count;
    }

    // Echo only after EOF. The server never inspects byte values, so a raw peer
    // may exercise the 64-byte boundary and zero bytes freely.
    const SendOutcome echo =
        send_block(peer, accumulated.data(), accumulated.size());
    if (!echo.ok) {
        std::cerr << "ERROR stage=echo error=" << echo.error << "\n";
        return 1;
    }

    if (!study_net::shutdown_send(peer, error)) {
        std::cerr << "ERROR stage=shutdown error=" << error << "\n";
        return 1;
    }

    std::cout << "TOTAL count=" << total << " hex=";
    print_hex(accumulated.data(), accumulated.size());
    std::cout << "\n";
    return 0;
}

int run_client(std::uint16_t port, Mode mode) {
    int error = 0;
    study_net::Socket socket = study_net::connect_loopback(port, error);
    if (!socket.valid()) {
        std::cerr << "ERROR stage=connect error=" << error << "\n";
        return 1;
    }

    // Fixed request bytes: numeric 0x41..0x46, i.e. "ABCDEF".
    const std::uint8_t payload[6] = {0x41u, 0x42u, 0x43u, 0x44u, 0x45u, 0x46u};

    SendOutcome outcome{true, 0, 0};
    switch (mode) {
    case Mode::one:
        outcome = send_block(socket, payload, 6);
        break;
    case Mode::two:
        outcome = send_block(socket, payload, 3);
        if (outcome.ok) {
            outcome = send_block(socket, payload + 3, 3, 3);
        }
        break;
    case Mode::bytes:
        for (std::size_t index = 0; index < 6 && outcome.ok; ++index) {
            outcome = send_block(socket, payload + index, 1, index);
        }
        break;
    }
    if (!outcome.ok) {
        std::cerr << "ERROR stage=send error=" << outcome.error << "\n";
        return 1;
    }

    // We stop offering request bytes but the socket stays open: the send
    // direction closes, the receive direction remains and the server can reply.
    if (!study_net::shutdown_send(socket, error)) {
        std::cerr << "ERROR stage=shutdown error=" << error << "\n";
        return 1;
    }

    std::uint8_t scratch[16];
    study_net::ByteBuffer<64> received;
    std::size_t total = 0;

    for (;;) {
        const study_net::StreamResult result =
            study_net::receive_some(socket, scratch, sizeof(scratch));
        if (result.status == study_net::StreamStatus::eof) {
            std::cout << "RECV eof offset=" << total << "\n";
            break;
        }
        if (result.status == study_net::StreamStatus::error) {
            std::cerr << "ERROR stage=receive error=" << result.error << "\n";
            return 1;
        }
        std::cout << "RECV offset=" << total
                  << " count=" << result.count << " hex=";
        print_hex(scratch, result.count);
        std::cout << "\n";
        if (!received.append(scratch, result.count)) {
            std::cerr << "ERROR stage=accumulate reason=overflow capacity=64\n";
            return 1;
        }
        total += result.count;
    }

    if (total != 6 || std::memcmp(received.data(), payload, 6) != 0) {
        std::cerr << "ERROR stage=verify reason="
                  << (total != 6 ? "length" : "content")
                  << " total=" << total << "\n";
        return 1;
    }

    std::cout << "VERIFIED 6" << std::endl;
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc == 2 && std::string_view(argv[1]) == "--help") {
        print_usage(std::cout);
        return 0;
    }
    if (argc < 2) {
        return cli_error("missing command");
    }

    const std::string_view command(argv[1]);
    if (command == "listen") {
        if (argc != 4) {
            return cli_error("listen requires PORT and READ_CAP");
        }
        std::uint16_t port = 0;
        unsigned int read_cap = 0;
        if (!parse_port(argv[2], true, port)) {
            return cli_error("invalid PORT (expected 0..65535)");
        }
        if (!parse_read_cap(argv[3], read_cap)) {
            return cli_error("invalid READ_CAP (expected 1..16)");
        }
        study_net::Runtime runtime;
        if (!runtime.ready()) {
            std::cerr << "ERROR stage=runtime error=" << runtime.error() << "\n";
            return 1;
        }
        return run_server(port, read_cap);
    }

    if (command == "connect") {
        if (argc != 4) {
            return cli_error("connect requires PORT and MODE");
        }
        std::uint16_t port = 0;
        Mode mode = Mode::one;
        if (!parse_port(argv[2], false, port)) {
            return cli_error("invalid PORT (expected 1..65535)");
        }
        if (!parse_mode(argv[3], mode)) {
            return cli_error("invalid MODE (expected one, two or bytes)");
        }
        study_net::Runtime runtime;
        if (!runtime.ready()) {
            std::cerr << "ERROR stage=runtime error=" << runtime.error() << "\n";
            return 1;
        }
        return run_client(port, mode);
    }

    return cli_error("unknown command");
}
