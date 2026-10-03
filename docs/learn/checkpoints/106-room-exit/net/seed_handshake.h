#ifndef STUDY_NET_SEED_HANDSHAKE_H
#define STUDY_NET_SEED_HANDSHAKE_H

// Blocking seed handshake driven over one already-established Connection.
//
// Scope and lifetime
// ------------------
// * Call each function at most once, on a fresh, exclusively-owned, connected
//   Connection. There is no rematch, no retry, and no reconnect here.
// * Receives are whatever Connection::next_frame() provides; because that call
//   blocks with no time bound, the handshake as a whole has no timeout. Each
//   send uses a cooperative now()+2s budget via Connection::send_frame().
// * The handshake never calls finish_sending() and never treats EOF as a
//   normal end; a read must produce exactly the expected frame.
// * On any network or protocol failure the Connection is closed and `agreed`
//   is left unchanged. On success the Connection stays open for later traffic.
//
// Message flow
// ------------
//   peer --HELLO--> host
//   host --OFFER--> peer
//   peer --ACK----> host
// The host dictates the Config in OFFER; the peer echoes it byte-for-byte in
// ACK. Host success means it read the ACK; peer success means its ACK send was
// accepted locally. Neither side claims the other observed completion, and
// there is no common-knowledge or simultaneous-success claim.

#include <chrono>

#include "net/seed_protocol.h"
#include "net/connection.h"

namespace study_net {

enum class HandshakeResult {
  ready,           // completed; `agreed` holds the negotiated Config
  invalid_local,   // host's proposed Config is out of policy (nothing sent)
  send_failed,     // a frame send did not complete within its budget
  receive_failed,  // a read did not yield a whole frame (EOF/error/...)
  protocol_error,  // a frame arrived but failed type/content validation
};

namespace seed_handshake_detail {

// Cooperative per-send budget: the deadline is recomputed for every send.
inline constexpr std::chrono::seconds kSendBudget{2};

// Send one frame under the per-call budget. Connection::send_frame() closes
// on I/O failure. The handshake caller also closes on any rejected send.
inline bool send_with_budget(Connection& connection, const Frame& frame) {
  const std::chrono::steady_clock::time_point deadline =
      std::chrono::steady_clock::now() + kSendBudget;
  const SendReport report = connection.send_frame(frame, deadline);
  return report.outcome == SendOutcome::complete;
}

}  // namespace seed_handshake_detail

// Host side: receive HELLO, propose `proposed` in OFFER, then require an
// identical ACK. `proposed` may alias `agreed`, so it is snapshotted before
// any output write.
inline HandshakeResult negotiate_host(Connection& connection,
                                      const Config& proposed,
                                      Config& agreed) {
  const Config local = proposed;  // snapshot before touching `agreed`
  if (!valid_config(local)) {
    return HandshakeResult::invalid_local;  // output and connection untouched
  }

  Frame hello{};
  const Connection::ReadReport hello_read = connection.next_frame(hello);
  if (hello_read.state != Connection::ReadState::frame) {
    connection.close();
    return HandshakeResult::receive_failed;
  }
  if (!valid_hello(hello)) {
    connection.close();
    return HandshakeResult::protocol_error;
  }

  Frame offer{};
  if (!encode_config(kTypeOffer, local, offer)) {
    connection.close();  // local was validated, so this path is unexpected
    return HandshakeResult::invalid_local;
  }
  if (!seed_handshake_detail::send_with_budget(connection, offer)) {
    connection.close();
    return HandshakeResult::send_failed;
  }

  Frame ack{};
  const Connection::ReadReport ack_read = connection.next_frame(ack);
  if (ack_read.state != Connection::ReadState::frame) {
    connection.close();
    return HandshakeResult::receive_failed;
  }
  Config echoed{};
  if (!decode_config(ack, kTypeAck, echoed)) {
    connection.close();
    return HandshakeResult::protocol_error;
  }
  if (!equal_config(echoed, local)) {
    connection.close();
    return HandshakeResult::protocol_error;
  }

  agreed = local;
  return HandshakeResult::ready;
}

// Peer side: send HELLO, accept the host's OFFER, and echo it back in ACK.
inline HandshakeResult negotiate_peer(Connection& connection, Config& agreed) {
  const Frame hello = make_hello();
  if (!seed_handshake_detail::send_with_budget(connection, hello)) {
    connection.close();
    return HandshakeResult::send_failed;
  }

  Frame offer{};
  const Connection::ReadReport offer_read = connection.next_frame(offer);
  if (offer_read.state != Connection::ReadState::frame) {
    connection.close();
    return HandshakeResult::receive_failed;
  }
  Config candidate{};
  if (!decode_config(offer, kTypeOffer, candidate)) {
    connection.close();
    return HandshakeResult::protocol_error;
  }

  Frame ack{};
  if (!encode_config(kTypeAck, candidate, ack)) {
    connection.close();  // candidate passed decode_config, so unexpected
    return HandshakeResult::protocol_error;
  }
  if (!seed_handshake_detail::send_with_budget(connection, ack)) {
    connection.close();
    return HandshakeResult::send_failed;
  }

  agreed = candidate;
  return HandshakeResult::ready;
}

}  // namespace study_net

#endif  // STUDY_NET_SEED_HANDSHAKE_H
