#pragma once
#include <cstdint>

namespace net {
// Winsock SOCKET is pointer-sized and unsigned; POSIX descriptors are signed int.
// Keep OS headers out of public headers to avoid Windows include-order conflicts.
#ifdef _WIN32
using NativeSocket = std::uintptr_t;
inline constexpr NativeSocket kInvalidSocket = ~NativeSocket{0};
inline constexpr bool socket_valid(NativeSocket fd) noexcept { return fd != kInvalidSocket; }
#else
using NativeSocket = int;
inline constexpr NativeSocket kInvalidSocket = -1;
inline constexpr bool socket_valid(NativeSocket fd) noexcept { return fd >= 0; }
#endif
} // namespace net
