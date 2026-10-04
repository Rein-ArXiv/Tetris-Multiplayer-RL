#ifndef STUDY_NET_CONNECTION_H
#define STUDY_NET_CONNECTION_H

// One TCP connection over the study_net layer: a blocking connect plus framing
// on the receive side, and a bounded nonblocking send on the send side.
//
// Ownership / lifetime
// --------------------
// * A Connection owns exactly one Socket (its OS handle) and one FrameParser
//   (its receive assembly state). Both are value members, so destroying the
//   Connection destroys them in reverse order: parser_ first, then socket_
//   closes the handle. Copying and moving are deleted so the handle is owned
//   by a single object and closed exactly once.
// * The Runtime needed by the socket is owned OUTSIDE and must outlive
//   this Connection. Connection never owns or references it.
// * No buffer handed to send_frame() outlives the call: the encoded frame is a
//   local and the call does not return until the send completes, fails, or the
//   deadline passes.
//
// Concurrency / blocking contract
// -------------------------------
// * Callers serialize all operations; there is no internal locking.
// * start() and the receive side of next_frame() are blocking with no time
//   bound. The deadline applies only to send_frame().
// * close() is only called after a synchronous call has returned; it is not a
//   way to cancel a call in progress from another thread.
// * This class performs no automatic reconnect or retry. A new start needs
//   a closed Connection (explicit close or a failure that closes it), even
//   when both directions have ended.

#include <chrono>
#include <cstddef>
#include <cstdint>

#include "net/framing.h"      // Frame, FrameParser
#include "net/send_budget.h"  // SendReport, SendOutcome
#include "net/socket.h"       // Socket

namespace study_net {

class Connection {
 public:
    // Result of start().
    //   started  : a fresh loopback connection is adopted and usable.
    //   busy     : a valid socket is already owned; nothing changed.
    //   io_error : connect failed; `error` holds the captured platform code.
    enum class StartState { started, busy, io_error };

    struct StartReport {
        StartState state;
        int error;
    };

    // Result of next_frame().
    //   frame          : `out` holds a whole decoded frame.
    //   eof            : clean stream boundary at a frame boundary; the send
    //                    direction remains usable.
    //   truncated      : EOF arrived with a partial frame pending.
    //   protocol_error : the parser rejected a malformed/oversized length.
    //   io_error       : a receive failed; `error` holds the platform code.
    //   closed         : the Connection has no active socket.
    enum class ReadState { frame, eof, truncated, protocol_error, io_error, closed };

    struct ReadReport {
        ReadState state;
        int error;
    };

    Connection() noexcept = default;
    ~Connection() = default;

    Connection(const Connection&) = delete;
    Connection& operator=(const Connection&) = delete;
    Connection(Connection&&) = delete;
    Connection& operator=(Connection&&) = delete;

    // Blocking connect to 127.0.0.1:port. Returns busy without changing any
    // state when a socket is already owned. On success the receive parser and
    // both direction flags are reset before the new socket is adopted. On
    // failure the candidate is discarded and `error` is the captured code.
    StartReport start(std::uint16_t port);

    // Close the owned socket and forget all per-connection state. Idempotent:
    // calling it on an already-closed Connection is a no-op. The socket is
    // reset first, so a later start() sees an empty Connection.
    void close() noexcept;

    // Adopt a fresh connected blocking socket. On rejection, caller keeps it.
    bool adopt(Socket& candidate) noexcept;

    // True while an OS socket handle is owned.
    bool active() const noexcept { return socket_.valid(); }

    // True once finish_sending() has half-closed our send direction.
    bool send_closed() const noexcept { return send_closed_; }

    // True once a clean EOF has been observed on the receive side.
    bool peer_eof() const noexcept { return peer_eof_; }

    // Unconsumed bytes held by the receive parser; nonzero at EOF means the
    // peer truncated a frame.
    std::size_t pending_bytes() const noexcept { return parser_.pending_bytes(); }

    // Encode and send one frame using a cooperative deadline (not hard realtime).
    //   * Rejected with invalid_request (no mutation) when inactive or when the
    //     send direction is already closed.
    //   * A frame that does not encode is invalid_request too, but the
    //     Connection is left open because that is a caller error.
    //   * The socket becomes nonblocking for the bounded send and, after a
    //     complete send, is restored to blocking.
    //   * Any failure or non-complete outcome closes the Connection. A complete
    //     send whose blocking-mode restore fails also closes.
    // The encoded buffer is local, so nothing outlives the call.
    SendReport send_frame(const Frame& frame,
                          std::chrono::steady_clock::time_point deadline);

    // Half-close the send direction, leaving the receive direction open.
    // Returns false with error == 0 for a locally invalid state (inactive) without making an OS call. If the shutdown call
    // fails, the Connection is closed and the captured error is preserved.
    bool finish_sending(int& error) noexcept;

    // Block until a whole frame is decoded (see ReadState for all outcomes).
    // `out` is left untouched unless the state is `frame`.
    ReadReport next_frame(Frame& out);

 private:
    // Owned OS handle. Socket::~Socket closes it; the Runtime must outlive us.
    Socket socket_{};
    // Owned receive assembly state (partial frames retained across reads).
    FrameParser parser_{};
    // True once finish_sending() has half-closed the send direction.
    bool send_closed_{false};
    // True once next_frame() observed a clean EOF (cached; socket stays open).
    bool peer_eof_{false};
};

}  // namespace study_net

#endif  // STUDY_NET_CONNECTION_H
