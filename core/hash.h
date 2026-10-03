#pragma once
#include <cstdint>
#include <cstddef>
#include <type_traits>

// FNV-1a 64-bit hash for quick state checksums
inline uint64_t fnv1a64(const void* data, size_t len, uint64_t seed = 14695981039346656037ull) {
    const uint8_t* ptr = static_cast<const uint8_t*>(data);
    uint64_t hash = seed;
    for (size_t i = 0; i < len; ++i) {
        hash ^= ptr[i];
        hash *= 1099511628211ull;
    }
    return hash;
}

// Fixed-width integers only. Objects, bools, pointers and padding are not a format.
template<typename T>
inline uint64_t fnv1a64_value(const T& v, uint64_t seed = 14695981039346656037ull) {
    static_assert(std::is_same_v<T, int32_t> || std::is_same_v<T, uint32_t> ||
                  std::is_same_v<T, int64_t> || std::is_same_v<T, uint64_t>,
                  "hash a fixed-width integer or an explicitly encoded byte sequence");
    using U = std::make_unsigned_t<T>;
    U value = static_cast<U>(v);
    for (size_t i = 0; i < sizeof(U); ++i) {
        seed ^= static_cast<uint8_t>(value & 0xffu);
        seed *= 1099511628211ull;
        value >>= 8;
    }
    return seed;
}

// Canonical player order: host first, peer second. Equal board hashes must not
// cancel as they do under XOR. This checksum is diagnostic, not authentication.
inline uint64_t hash_player_pair(uint64_t host, uint64_t peer) {
    constexpr uint8_t domain[] = {'D', 'U', 'E', 'L', 1};
    uint64_t hash = fnv1a64(domain, sizeof(domain));
    hash = fnv1a64_value(host, hash);
    return fnv1a64_value(peer, hash);
}
