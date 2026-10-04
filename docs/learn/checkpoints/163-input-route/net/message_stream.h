#ifndef STUDY_NET_MESSAGE_STREAM_H
#define STUDY_NET_MESSAGE_STREAM_H

// MessageStream: a thin adapter between a reassembled WebSocket message and
// the bounded study FrameParser.
//
// Contract:
//   * This class is NOT a WebSocket parser. accept() is called only after the
//     transport (e.g. Boost.Beast) has reassembled one complete WS message.
//     `binary` says whether that message was binary; text is rejected.
//   * Message boundaries are deliberately transparent to framing. The parser
//     keeps an incomplete frame tail ACROSS accept() calls, so one game frame
//     may span two WebSocket messages and is never reset at a message edge.
//   * accept() has streaming prefix semantics: frames handed to `sink` before a
//     later failure are already delivered and cannot be rolled back.
//   * Any rejection latches sticky failure; there is no reset path.

#include <algorithm>
#include <cstddef>
#include <cstdint>

#include "net/framing.h"

namespace study_net {

class MessageStream {
 public:
  // `message_limit` is the maximum accepted WebSocket message size in bytes.
  explicit MessageStream(std::size_t message_limit) noexcept
      : message_limit_(message_limit) {}

  // Consume one complete, reassembled WebSocket message.
  //
  // Returns true when the whole message was fed (not necessarily when every
  // frame was delivered) and false on any rejection or latched failure.
  //
  // Rejections (all sticky, all checked BEFORE any sink callback):
  //   * text messages, including empty text;
  //   * size > message_limit_;
  //   * data == nullptr while size != 0.
  // An empty binary message is accepted while not failed.
  //
  // Accepted bytes are supplied to the parser in chunks of at most
  // kReceiveCapacity - kMaxFrameBytes, draining to need_more after each chunk.
  // `sink` is invoked as sink(const Frame&) -> bool; a false result latches
  // failure. Sink exceptions are NOT caught: they propagate to the caller and
  // are the caller's responsibility. A frame has already been removed when
  // the sink runs: after an exception the caller must discard this stream.
  template <class Sink>
  bool accept(bool binary, const std::uint8_t* data, std::size_t size,
              Sink&& sink) {
    if (failed_) {
      return false;
    }
    // Reject text (also empty text) before touching the parser.
    if (!binary) {
      failed_ = true;
      return false;
    }
    // Reject messages larger than the configured bound.
    if (size > message_limit_) {
      failed_ = true;
      return false;
    }
    // Reject a null buffer that claims to carry bytes.
    if (data == nullptr && size != 0) {
      failed_ = true;
      return false;
    }
    // Empty binary message: nothing to feed, accepted while not failed.
    if (size == 0) {
      return true;
    }

    constexpr std::size_t kChunkBytes = kReceiveCapacity - kMaxFrameBytes;
    static_assert(kChunkBytes > 0, "chunk capacity must be positive");
    static_assert(kChunkBytes <= kReceiveCapacity,
                  "chunk must fit the parser receive buffer");

    std::size_t offset = 0;
    while (offset < size) {
      const std::size_t chunk = std::min(kChunkBytes, size - offset);
      if (!parser_.append(data + offset, chunk)) {
        failed_ = true;
        return false;
      }
      offset += chunk;

      // Drain until the parser needs more input; repeat after every chunk.
      for (;;) {
        Frame frame{};
        const ParseStatus status = parser_.next(frame);
        if (status == ParseStatus::need_more) {
          break;
        }
        if (status == ParseStatus::error) {
          failed_ = true;
          return false;
        }
        if (!sink(static_cast<const Frame&>(frame))) {
          failed_ = true;
          return false;
        }
      }
    }
    return true;
  }

  // Latch and report the final state. Call exactly once, only on real transport
  // EOF, never per message. Returns false if a failure was latched or an
  // incomplete frame tail remains; otherwise true.
  bool finish() noexcept {
    if (failed_ || parser_.pending_bytes() != 0) {
      failed_ = true;
      return false;
    }
    return true;
  }

  bool failed() const noexcept { return failed_; }

  std::size_t pending_bytes() const noexcept {
    return parser_.pending_bytes();
  }

 private:
  FrameParser parser_{};
  bool failed_{false};
  std::size_t message_limit_{};
};

}  // namespace study_net

#endif  // STUDY_NET_MESSAGE_STREAM_H
