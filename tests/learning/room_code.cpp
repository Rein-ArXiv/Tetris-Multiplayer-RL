#include "server/room_code.h"
#include <array>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <limits>
#include <set>
#if defined(ROOM_RANDOM_WRAP)
#include <sys/random.h>
int random_mode = 0;
extern "C" ssize_t __real_getrandom(void*, size_t, unsigned int);
extern "C" ssize_t __wrap_getrandom(void* out, size_t size, unsigned int flags) {
    if (random_mode == 0) return __real_getrandom(out, size, flags);
    if (size != 4 || flags != GRND_NONBLOCK) return -1;
    if (random_mode == 1) { std::memset(out, 0, size); return 4; }
    if (random_mode == 2) { std::memset(out, 0xff, size); return 3; }
    errno = random_mode == 3 ? EAGAIN : EINTR;
    return -1;
}
#endif
#define CHECK(x) do { if (!(x)) { std::fprintf(stderr, "line %d: %s\n", __LINE__, #x); return 1; } } while (false)
int main() {
    using namespace relay;
    CHECK(roomCodeFromWord(0) == "AAAAA");
    CHECK(roomCodeFromWord(UINT32_MAX) == "99999");
    for (unsigned position = 0; position < 5; ++position) {
        for (unsigned digit = 0; digit < 32; ++digit) {
            std::string want(5, 'A'); want[position] = kRoomCodeAlphabet[digit];
            CHECK(roomCodeFromWord(digit << (position * 5)) == want);
        }
    }
    CHECK(roomCodeFromWord(0xfe000000u) == "AAAAA");
    unsigned calls = 0;
    auto occupied = [](const std::string& code) { return code == "AAAAA"; };
    auto candidate = selectRoomCode([&]() -> std::optional<uint32_t> { return calls++; }, occupied);
    CHECK(candidate && *candidate == "BAAAA" && calls == 2);
    calls = 0;
    candidate = selectRoomCode([&]() -> std::optional<uint32_t> { ++calls; return 0; }, occupied);
    CHECK(!candidate && calls == 32);
    calls = 0;
    candidate = selectRoomCode([&]() -> std::optional<uint32_t> { ++calls; return {}; }, occupied);
    CHECK(!candidate && calls == 1);
    CHECK(roomCodeRandomWord().has_value()); // Check OS integration, not statistical security.
#if defined(ROOM_RANDOM_WRAP)
    random_mode = 1; CHECK(roomCodeRandomWord() == std::optional<uint32_t>{0});
    for (random_mode = 2; random_mode <= 4; ++random_mode) CHECK(!roomCodeRandomWord());
#endif
    std::puts("room code: 160 digit positions, zero/upper bits, collision retry32, source failure; OS source/failure contract");
}
