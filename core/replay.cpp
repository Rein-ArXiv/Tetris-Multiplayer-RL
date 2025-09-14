#include "replay.h"
#include <fstream>

namespace ReplayIO {
    bool Save(const std::string& path, const ReplayData& rp) {
        std::ofstream f(path, std::ios::out | std::ios::trunc);
        if (!f) return false;
        f << "seed " << rp.seed << "\n";
        f << "ticks " << rp.frames.size() << "\n";
        for (size_t i = 0; i < rp.frames.size(); ++i) {
            const auto& fr = rp.frames[i];
            f << i << " " << static_cast<unsigned>(fr.p1) << " " << static_cast<unsigned>(fr.p2) << "\n";
        }
        return true;
    }

    bool Load(const std::string& path, ReplayData& out) {
        std::ifstream f(path);
        if (!f) return false;
        std::string key; size_t ticks = 0; uint64_t seed = 0;
        if (!(f >> key >> seed)) return false;
        if (key != "seed") return false;
        if (!(f >> key >> ticks)) return false;
        if (key != "ticks") return false;
        out.seed = seed;
        out.frames.clear();
        out.frames.resize(ticks);
        size_t idx; unsigned p1, p2;
        for (size_t i = 0; i < ticks && (f >> idx >> p1 >> p2); ++i) {
            if (idx >= ticks) break;
            out.frames[idx].p1 = static_cast<uint8_t>(p1 & 0xFFu);
            out.frames[idx].p2 = static_cast<uint8_t>(p2 & 0xFFu);
        }
        return true;
    }
}

