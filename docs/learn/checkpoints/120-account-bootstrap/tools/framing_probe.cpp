// tools/framing_probe.cpp
//
// Bounded framing probe for the teaching study.
//
//   framing_probe listen  PORT READ_CAP   READ_CAP in [1, 16]
//   framing_probe connect PORT CHUNK      CHUNK    in [1, 16], PORT != 0
//   framing_probe --help
//
// Exit codes: 0 success / help, 1 runtime or protocol failure, 2 invalid CLI.
//
// Wire frame: [LEN:u16 LE][TYPE:u8][PAYLOAD:LEN-1], no checksum.
// Application types: request = 1, reply = 2. The parser does not validate
// types; the application does. This probe is deliberately tiny and single
// shot; it is not a general concurrent server.

#include <algorithm>
#include <array>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

#include "net/byte_buffer.h"
#include "net/framing.h"
#include "net/socket.h"
#include "net/stream.h"

namespace {

constexpr std::uint8_t kRequestType = 1;
constexpr std::uint8_t kReplyType = 2;
constexpr std::size_t kReadWindow = 16;

void print_usage(std::FILE* out) {
  std::fprintf(out,
               "usage:\n"
               "  framing_probe listen PORT READ_CAP   (READ_CAP 1..16)\n"
               "  framing_probe connect PORT CHUNK     (CHUNK 1..16, PORT != 0)\n"
               "  framing_probe --help\n");
}

bool parse_u32_strict(const char* text, unsigned& out) {
  if (text == nullptr || *text == '\0') {
    return false;
  }
  unsigned long long value = 0;
  const char* begin = text;
  const char* end = text + std::strlen(text);
  const std::from_chars_result result = std::from_chars(begin, end, value, 10);
  if (result.ec != std::errc() || result.ptr != end) {
    return false;
  }
  if (value > 0xFFFFFFFFull) {
    return false;
  }
  out = static_cast<unsigned>(value);
  return true;
}

int fail(const char* stage, const char* reason) {
  std::fprintf(stderr, "ERROR stage=%s reason=%s\n", stage, reason);
  std::fflush(stderr);
  return 1;
}

int fail_error(const char* stage, const char* reason, int error) {
  std::fprintf(stderr, "ERROR stage=%s reason=%s error=%d\n", stage, reason, error);
  std::fflush(stderr);
  return 1;
}

void print_frame_hex(const study_net::Frame& frame) {
  static const char kDigits[] = "0123456789abcdef";
  char hex[2 * study_net::kMaxPayloadBytes + 1];
  std::size_t n = 0;
  for (std::size_t i = 0; i < frame.size; ++i) {
    hex[n++] = kDigits[(frame.payload[i] >> 4) & 0x0Fu];
    hex[n++] = kDigits[frame.payload[i] & 0x0Fu];
  }
  hex[n] = '\0';
  std::printf("FRAME %zu %s\n", frame.size, hex);
  std::fflush(stdout);
}

// Send exactly `count` bytes, at most `chunk` per call, advancing by the
// positive progress reported by send_some. Blocking; no sleep, no MSG_WAITALL.
bool send_all(study_net::Socket& sock, const std::uint8_t* data, std::size_t count,
              std::size_t chunk, const char* stage) {
  std::size_t offset = 0;
  while (offset < count) {
    const std::size_t want = (std::min)(chunk, count - offset);
    const study_net::StreamResult result =
        study_net::send_some(sock, data + offset, want);
    if (result.status == study_net::StreamStatus::progress) {
      if (result.count == 0) {
        std::fprintf(stderr, "ERROR stage=%s reason=no_progress sent=%zu\n", stage,
                     offset);
        std::fflush(stderr);
        return false;
      }
      offset += result.count;
    } else if (result.status == study_net::StreamStatus::error) {
      std::fprintf(stderr, "ERROR stage=%s reason=send error=%d sent=%zu\n", stage,
                   result.error, offset);
      std::fflush(stderr);
      return false;
    } else {
      std::fprintf(stderr, "ERROR stage=%s reason=unexpected_eof sent=%zu\n", stage,
                   offset);
      std::fflush(stderr);
      return false;
    }
  }
  return true;
}

bool same_payload(const study_net::Frame& frame, const std::uint8_t* data,
                  std::size_t size) {
  if (frame.size != size) {
    return false;
  }
  for (std::size_t i = 0; i < size; ++i) {
    if (frame.payload[i] != data[i]) {
      return false;
    }
  }
  return true;
}

study_net::Frame make_request(const std::uint8_t* data, std::size_t size) {
  study_net::Frame frame{};
  frame.type = kRequestType;
  frame.size = size;
  for (std::size_t i = 0; i < size; ++i) {
    frame.payload[i] = data[i];
  }
  return frame;
}

int run_server(unsigned port_arg, std::size_t read_cap) {
  int error = 0;
  std::uint16_t actual_port = 0;
  study_net::Socket listener = study_net::listen_loopback(
      static_cast<std::uint16_t>(port_arg), actual_port, error);
  if (!listener.valid()) {
    return fail_error("listen", "listen_loopback", error);
  }
  std::printf("LISTEN %u\n", static_cast<unsigned>(actual_port));
  std::fflush(stdout);

  study_net::Socket connection = study_net::accept_one(listener, error);
  listener.reset();
  if (!connection.valid()) {
    return fail_error("accept", "accept_one", error);
  }

  study_net::FrameParser parser;
  std::uint8_t buffer[kReadWindow];
  std::size_t frames = 0;

  for (;;) {
    const study_net::StreamResult result =
        study_net::receive_some(connection, buffer, read_cap);
    if (result.status == study_net::StreamStatus::error) {
      return fail_error("receive", "receive_some", result.error);
    }
    if (result.status == study_net::StreamStatus::eof) {
      break;
    }
    if (result.count == 0) {
      return fail("receive", "no_progress");
    }
    if (!parser.append(buffer, result.count)) {
      return fail("append", "overflow");
    }
    for (;;) {
      study_net::Frame frame{};
      const study_net::ParseStatus status = parser.next(frame);
      if (status == study_net::ParseStatus::need_more) {
        break;
      }
      if (status == study_net::ParseStatus::error) {
        return fail("parse", "malformed_length");
      }
      if (frame.type != kRequestType) {
        return fail("parse", "unknown_type");
      }
      study_net::Frame reply{};
      reply.type = kReplyType;
      reply.size = frame.size;
      reply.payload = frame.payload;
      study_net::EncodedFrame encoded{};
      if (!study_net::encode_frame(reply, encoded)) {
        return fail("encode", "payload_too_large");
      }
      if (!send_all(connection, encoded.bytes.data(), encoded.size, encoded.size,
                    "send_reply")) {
        return 1;
      }
      print_frame_hex(frame);
      ++frames;
    }
  }

  if (parser.pending_bytes() != 0) {
    return fail("eof", "truncated_frame");
  }
  int shut_error = 0;
  if (!study_net::shutdown_send(connection, shut_error)) {
    return fail_error("shutdown", "shutdown_send", shut_error);
  }
  std::printf("FRAMES %zu\n", frames);
  std::fflush(stdout);
  return 0;
}

int run_client(unsigned port_arg, std::size_t chunk) {
  int error = 0;
  study_net::Socket socket =
      study_net::connect_loopback(static_cast<std::uint16_t>(port_arg), error);
  if (!socket.valid()) {
    return fail_error("connect", "connect_loopback", error);
  }

  const std::array<std::uint8_t, 3> request_a{{0x41, 0x42, 0x43}};
  const std::array<std::uint8_t, 3> request_c{{0x00, 0xFF, 0x2A}};

  study_net::EncodedFrame encoded_a{};
  study_net::EncodedFrame encoded_b{};
  study_net::EncodedFrame encoded_c{};
  if (!study_net::encode_frame(make_request(request_a.data(), request_a.size()),
                               encoded_a) ||
      !study_net::encode_frame(make_request(nullptr, 0), encoded_b) ||
      !study_net::encode_frame(make_request(request_c.data(), request_c.size()),
                               encoded_c)) {
    return fail("encode", "request");
  }

  study_net::ByteBuffer<3 * study_net::kMaxFrameBytes> batch;
  batch.append(encoded_a.bytes.data(), encoded_a.size);
  batch.append(encoded_b.bytes.data(), encoded_b.size);
  batch.append(encoded_c.bytes.data(), encoded_c.size);

  if (!send_all(socket, batch.data(), batch.size(), chunk, "send_requests")) {
    return 1;
  }
  // Intentionally no shutdown yet: the replies must arrive on their own
  // boundary before the client signals EOF.

  study_net::FrameParser parser;
  std::uint8_t buffer[kReadWindow];
  std::size_t replies = 0;
  bool shutdown_done = false;

  for (;;) {
    const study_net::StreamResult result =
        study_net::receive_some(socket, buffer, kReadWindow);
    if (result.status == study_net::StreamStatus::error) {
      return fail_error("receive", "receive_some", result.error);
    }
    if (result.status == study_net::StreamStatus::eof) {
      break;
    }
    if (result.count == 0) {
      return fail("receive", "no_progress");
    }
    if (!parser.append(buffer, result.count)) {
      return fail("append", "overflow");
    }
    for (;;) {
      study_net::Frame frame{};
      const study_net::ParseStatus status = parser.next(frame);
      if (status == study_net::ParseStatus::need_more) {
        break;
      }
      if (status == study_net::ParseStatus::error) {
        return fail("parse", "malformed_length");
      }
      if (replies >= 3) {
        return fail("reply", "extra_frame");
      }
      if (frame.type != kReplyType) {
        return fail("reply", "unexpected_type");
      }
      const bool payload_ok =
          replies == 0
              ? same_payload(frame, request_a.data(), request_a.size())
              : (replies == 1 ? frame.size == 0
                              : same_payload(frame, request_c.data(),
                                             request_c.size()));
      if (!payload_ok) {
        return fail("reply", "payload_mismatch");
      }
      ++replies;
      if (replies == 3 && !shutdown_done) {
        int shut_error = 0;
        if (!study_net::shutdown_send(socket, shut_error)) {
          return fail_error("shutdown", "shutdown_send", shut_error);
        }
        shutdown_done = true;
      }
    }
  }

  if (parser.pending_bytes() != 0) {
    return fail("eof", "truncated_tail");
  }
  if (replies != 3) {
    return fail("eof", "missing_replies");
  }
  std::printf("VERIFIED 3 FRAMES\n");
  std::fflush(stdout);
  return 0;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc == 2 && std::strcmp(argv[1], "--help") == 0) {
    print_usage(stdout);
    return 0;
  }
  if (argc != 4) {
    print_usage(stderr);
    return 2;
  }

  const std::string mode = argv[1];
  unsigned port = 0;
  unsigned cap = 0;

  if (mode == "listen") {
    if (!parse_u32_strict(argv[2], port) || port > 65535u) {
      std::fprintf(stderr, "ERROR stage=cli reason=bad_port\n");
      return 2;
    }
    if (!parse_u32_strict(argv[3], cap) || cap < 1u || cap > 16u) {
      std::fprintf(stderr, "ERROR stage=cli reason=bad_read_cap\n");
      return 2;
    }
    // Runtime is created before, and outlives, every socket.
    study_net::Runtime runtime;
    if (!runtime.ready()) {
      return fail_error("runtime", "not_ready", runtime.error());
    }
    return run_server(port, static_cast<std::size_t>(cap));
  }

  if (mode == "connect") {
    if (!parse_u32_strict(argv[2], port) || port < 1u || port > 65535u) {
      std::fprintf(stderr, "ERROR stage=cli reason=bad_port\n");
      return 2;
    }
    if (!parse_u32_strict(argv[3], cap) || cap < 1u || cap > 16u) {
      std::fprintf(stderr, "ERROR stage=cli reason=bad_chunk\n");
      return 2;
    }
    study_net::Runtime runtime;
    if (!runtime.ready()) {
      return fail_error("runtime", "not_ready", runtime.error());
    }
    return run_client(port, static_cast<std::size_t>(cap));
  }

  print_usage(stderr);
  return 2;
}
