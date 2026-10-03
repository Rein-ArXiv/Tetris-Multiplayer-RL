#include "room_code.h"

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <bcrypt.h>
#elif defined(__linux__)
#include <sys/random.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

namespace relay {

std::optional<std::uint32_t> roomCodeRandomWord() noexcept {
    unsigned char bytes[4]{};
#if defined(_WIN32)
    if (BCryptGenRandom(nullptr, bytes, sizeof(bytes),
                        BCRYPT_USE_SYSTEM_PREFERRED_RNG) != 0) return std::nullopt;
#elif defined(__linux__)
    // Refuse creation if the OS source is not ready. Even short/error reads fail
    // closed, rather than waiting or substituting a predictable candidate.
    if (::getrandom(bytes, sizeof(bytes), GRND_NONBLOCK) != sizeof(bytes))
        return std::nullopt;
#else
    int flags = O_RDONLY;
#ifdef O_CLOEXEC
    flags |= O_CLOEXEC;
#endif
    const int fd = ::open("/dev/urandom", flags);
    if (fd < 0) return std::nullopt;
    const auto received = ::read(fd, bytes, sizeof(bytes));
    ::close(fd);
    if (received != sizeof(bytes)) return std::nullopt;
#endif
    return std::uint32_t(bytes[0]) | (std::uint32_t(bytes[1]) << 8) |
           (std::uint32_t(bytes[2]) << 16) | (std::uint32_t(bytes[3]) << 24);
}

} // namespace relay
