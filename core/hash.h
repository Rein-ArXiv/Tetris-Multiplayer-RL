#pragma once
#include <cstdint>
#include <cstddef>

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

template<typename T>
inline uint64_t fnv1a64_value(const T& v, uint64_t seed = 14695981039346656037ull) {
    return fnv1a64(&v, sizeof(T), seed);
}

