// Winsock2 must be included before any other Windows header.
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#else
#include <cerrno>
#include <sys/socket.h>
#include <sys/types.h>
#endif

#include "net/receive_socket.h"
#include "net/stream.h"

namespace study_net {
namespace {

int invalid_argument_error() noexcept {
#ifdef _WIN32
  return WSAEINVAL;
#else
  return EINVAL;
#endif
}

}  // namespace

ReceiveAttempt try_receive(Socket& socket, std::uint8_t* data,
                           std::size_t count) noexcept {
  if (!socket.valid() || data == nullptr || count == 0) {
    return ReceiveAttempt{ReceiveState::error, 0, invalid_argument_error()};
  }

  const int request = io_chunk_size(count);
  const auto received =
      ::recv(socket.native(), reinterpret_cast<char*>(data), request, 0);

  if (received > 0) {
    return ReceiveAttempt{ReceiveState::progress,
                          static_cast<std::size_t>(received), 0};
  }
  if (received == 0) {
    return ReceiveAttempt{ReceiveState::eof, 0, 0};
  }

#ifdef _WIN32
  const int err = WSAGetLastError();
  if (err == WSAEWOULDBLOCK) {
    return ReceiveAttempt{ReceiveState::would_block, 0, err};
  }
  return ReceiveAttempt{ReceiveState::error, 0, err};
#else
  const int err = errno;
  if (err == EAGAIN || err == EWOULDBLOCK) {
    return ReceiveAttempt{ReceiveState::would_block, 0, err};
  }
  if (err == EINTR) {
    return ReceiveAttempt{ReceiveState::interrupted, 0, err};
  }
  return ReceiveAttempt{ReceiveState::error, 0, err};
#endif
}

}  // namespace study_net
