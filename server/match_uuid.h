#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <random>
#include <string>

namespace relay {

// Unique result key; it is an identifier, not an authentication secret.
inline std::string new_match_uuid()
{
    static std::atomic<uint64_t> counter{1};
    static const uint64_t boot_random = [] {
        std::random_device rd;
        uint64_t v = (static_cast<uint64_t>(rd()) << 32) ^ rd();
        return v ? v : 0x9e3779b97f4a7c15ULL;
    }();

    const uint64_t now = static_cast<uint64_t>(
        std::chrono::high_resolution_clock::now().time_since_epoch().count());
    uint64_t a = now ^ boot_random ^ counter.fetch_add(1, std::memory_order_relaxed);
    uint64_t b = a + 0x9e3779b97f4a7c15ULL;
    auto mix = [](uint64_t x) {
        x ^= x >> 30; x *= 0xbf58476d1ce4e5b9ULL;
        x ^= x >> 27; x *= 0x94d049bb133111ebULL;
        return x ^ (x >> 31);
    };
    a = mix(a);
    b = mix(b ^ boot_random);

    char out[33];
    std::snprintf(out, sizeof(out), "%016llx%016llx",
                  static_cast<unsigned long long>(a),
                  static_cast<unsigned long long>(b));
    return std::string(out, 32);
}

}  // namespace relay
