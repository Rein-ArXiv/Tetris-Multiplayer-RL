#include "replay.h"
#include "input.h"
#include <charconv>
#include <fstream>
#include <iomanip>
#include <locale>
#include <new>
#include <utility>

namespace {
// A uint64_t decimal has at most 20 digits. One extra character detects an
// overlong token without allocating storage proportional to untrusted input.
bool read_decimal(std::istream& in, uint64_t& value) {
    std::string token;
    if (!(in >> std::setw(21) >> token) || token.size() > 20) return false;
    const auto parsed = std::from_chars(token.data(), token.data() + token.size(), value);
    return parsed.ec == std::errc{} && parsed.ptr == token.data() + token.size();
}
bool read_key(std::istream& in, const char* expected) {
    std::string token;
    return bool(in >> std::setw(6) >> token) && token == expected;
}
}

namespace ReplayIO {
    bool Save(const std::string& path, const ReplayData& rp) {
        if (rp.frames.size() > kMaxTicks) return false;
        for (const auto& fr : rp.frames)
            if (!isValidInputMask(fr.p1) || !isValidInputMask(fr.p2)) return false;
        std::ofstream f(path, std::ios::out | std::ios::trunc);
        if (!f) return false;
        f.imbue(std::locale::classic());
        f << "seed " << rp.seed << "\n";
        f << "ticks " << rp.frames.size() << "\n";
        for (size_t i = 0; i < rp.frames.size(); ++i) {
            const auto& fr = rp.frames[i];
            f << i << " " << static_cast<unsigned>(fr.p1) << " " << static_cast<unsigned>(fr.p2) << "\n";
        }
        f.flush();
        return bool(f);
    }

    bool Load(const std::string& path, ReplayData& out) {
        std::ifstream f(path, std::ios::binary);
        if (!f) return false;
        f.imbue(std::locale::classic());
        f.seekg(0, std::ios::end);
        const auto bytes = f.tellg();
        if (bytes < 0 || static_cast<uint64_t>(bytes) > kMaxFileBytes) return false;
        f.seekg(0);
        uint64_t ticks = 0;
        ReplayData candidate;
        if (!read_key(f, "seed") || !read_decimal(f, candidate.seed) ||
            !read_key(f, "ticks") || !read_decimal(f, ticks) || ticks > kMaxTicks)
            return false;
        try {
            candidate.frames.reserve(static_cast<size_t>(ticks));
            for (uint64_t i = 0; i < ticks; ++i) {
                uint64_t idx = 0, p1 = 0, p2 = 0;
                if (!read_decimal(f, idx) || idx != i ||
                    !read_decimal(f, p1) || !read_decimal(f, p2) ||
                    !isValidInputMask(p1) || !isValidInputMask(p2)) return false;
                candidate.frames.push_back({static_cast<uint8_t>(p1), static_cast<uint8_t>(p2)});
            }
        } catch (const std::bad_alloc&) {
            return false;
        }
        f >> std::ws;
        if (!f.eof() || f.bad()) return false;
        out = std::move(candidate); // Publish only a complete, validated replay.
        return true;
    }
}
