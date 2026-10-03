#include "net/connection.h"

#include "net/send_socket.h"  // set_nonblocking, send_bounded
#include "net/stream.h"       // StreamStatus, receive_some, shutdown_send

#include <utility>  // std::move

namespace study_net {

namespace {
// Receive scratch size: an incomplete parser tail is always < kMaxFrameBytes,
// so a bounded read of at most 16 bytes keeps append within kReceiveCapacity.
constexpr std::size_t kReceiveScratchBytes = 16;
}  // namespace

Connection::StartReport Connection::start(std::uint16_t port) {
    // A live socket means this Connection is already in use; refuse to touch it.
    if (socket_.valid()) {
        return StartReport{StartState::busy, 0};
    }

    int error = 0;
    Socket candidate = connect_loopback(port, error);
    if (!candidate.valid()) {
        // `candidate` is invalid and closes nothing; the captured error is kept.
        return StartReport{StartState::io_error, error};
    }

    // Fresh receive state for the fresh connection: no stale partial frame or
    // half-closed flag may survive a previous session.
    parser_ = FrameParser{};
    send_closed_ = false;
    peer_eof_ = false;

    // Adopt ownership of the connected handle exactly once (move transfers it).
    socket_ = std::move(candidate);
    return StartReport{StartState::started, 0};
}

bool Connection::adopt(Socket& candidate) noexcept {
    if (active() || !candidate.valid()) return false;
    parser_ = FrameParser{};
    send_closed_ = false;
    peer_eof_ = false;
    socket_ = std::move(candidate);
    return true;
}

void Connection::close() noexcept {
    // Reset the socket first so the handle is invalidated before anything else,
    // then clear the parser and flags. Safe to repeat.
    socket_.reset();
    parser_ = FrameParser{};
    send_closed_ = false;
    peer_eof_ = false;
}

SendReport Connection::send_frame(const Frame& frame,
                                  std::chrono::steady_clock::time_point deadline) {
    // Locally invalid requests never mutate the Connection.
    if (!active() || send_closed_) {
        return SendReport{SendOutcome::invalid_request, 0, 0};
    }

    EncodedFrame encoded{};
    if (!encode_frame(frame, encoded)) {
        // A frame that cannot be encoded is a caller error, not an I/O failure,
        // so the Connection is left open.
        return SendReport{SendOutcome::invalid_request, 0, 0};
    }

    int error = 0;
    if (!set_nonblocking(socket_, true, error)) {
        close();
        return SendReport{SendOutcome::error, 0, error};
    }

    // `encoded` is local; the bounded send returns before it goes out of scope.
    const SendReport report =
        send_bounded(socket_, encoded.bytes.data(), encoded.size, deadline);
    if (report.outcome != SendOutcome::complete) {
        // The accepted prefix is known, but a frame may be incomplete at the
        // peer. End this connection and preserve the original send report.
        close();
        return report;
    }

    // The whole frame was accepted; restore blocking mode for later calls. If
    // that fails the stream is unusable, so close but keep the accepted count.
    if (!set_nonblocking(socket_, false, error)) {
        close();
        return SendReport{SendOutcome::error, report.accepted, error};
    }
    return report;
}

bool Connection::finish_sending(int& error) noexcept {
    if (!active()) {
        // Pure local invalid state: no OS call, no captured error.
        error = 0;
        return false;
    }
    if (send_closed_) {
        error = 0;
        return true;
    }

    if (!shutdown_send(socket_, error)) {
        // shutdown_send already stored the immediate platform error.
        close();
        return false;
    }
    send_closed_ = true;
    return true;
}

Connection::ReadReport Connection::next_frame(Frame& out) {
    if (!socket_.valid()) {
        return ReadReport{ReadState::closed, 0};
    }
    if (peer_eof_) {
        // Cached EOF: the receive side ended, but the socket stays open so the
        // send direction can still be used.
        return ReadReport{ReadState::eof, 0};
    }

    for (;;) {
        // parser_.next leaves `out` untouched for need_more/error.
        const ParseStatus status = parser_.next(out);
        if (status == ParseStatus::frame) {
            return ReadReport{ReadState::frame, 0};
        }
        if (status == ParseStatus::error) {
            // This parser has no resynchronization rule after a malformed length.
            close();
            return ReadReport{ReadState::protocol_error, 0};
        }

        // need_more: pull a bounded chunk and feed the parser. An incomplete
        // tail is < kMaxFrameBytes, so 16 bytes never overflow the parser.
        std::uint8_t scratch[kReceiveScratchBytes];
        const StreamResult result =
            receive_some(socket_, scratch, sizeof(scratch));
        if (result.status == StreamStatus::error) {
            const int error = result.error;
            close();
            return ReadReport{ReadState::io_error, error};
        }
        if (result.status == StreamStatus::eof) {
            if (parser_.pending_bytes() != 0) {
                // EOF with a partial frame pending means the peer truncated.
                close();
                return ReadReport{ReadState::truncated, 0};
            }
            // Clean boundary: cache EOF but keep the socket for sending.
            peer_eof_ = true;
            return ReadReport{ReadState::eof, 0};
        }

        // StreamStatus::progress: hand the bytes over and retry. A rejected
        // append (handler overflow) latches the parser, so treat it as a
        // protocol error and close.
        if (!parser_.append(scratch, result.count)) {
            close();
            return ReadReport{ReadState::protocol_error, 0};
        }
    }
}

}  // namespace study_net
