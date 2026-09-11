#include "opponents.h"
#include <algorithm>
#include <charconv>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <unordered_map>
#include <unordered_set>

namespace bot {
namespace {
std::string trim(const std::string& text) {
    auto a = text.find_first_not_of(" \t\r\n");
    return a == std::string::npos ? "" : text.substr(a, text.find_last_not_of(" \t\r\n") - a + 1);
}
std::vector<std::string> fields(std::string line) {
    line = line.substr(0, line.find('#'));
    std::vector<std::string> out;
    size_t begin = 0;
    while (true) {
        auto end = line.find('|', begin);
        out.push_back(trim(line.substr(begin, end == std::string::npos ? end : end - begin)));
        if (end == std::string::npos) break;
        begin = end + 1;
    }
    return out;
}
bool number(const std::string& s, int& out, int low, int high) {
    int value = 0;
    auto r = std::from_chars(s.data(), s.data() + s.size(), value);
    if (r.ec != std::errc{} || r.ptr != s.data() + s.size() || value < low || value > high) return false;
    out = value;
    return true;
}
bool valid_id(const std::string& id) {
    return !id.empty() && id.size() <= 32 && id.find_first_not_of(
        "abcdefghijklmnopqrstuvwxyz0123456789_-") == std::string::npos;
}
std::string key(const std::filesystem::path& p) { return p.lexically_normal().generic_u8string(); }
}
int clamp_input_interval(int ticks) { return std::clamp(ticks, 1, 30); }

std::vector<Opponent> discover_opponents(const char* characters, const char* legacy) {
    std::vector<Opponent> roster;
    std::unordered_set<std::string> ids, configuredModels;
    std::ifstream file(characters);
    std::string line;
    while (std::getline(file, line)) {
        auto f = fields(line);
        if (f[0].empty()) continue;
        Opponent entry;
        if (f.size() != 9 || !valid_id(f[0]) || f[1].empty() || f[1].size() > 96 || f[2].empty() ||
            !number(f[6], entry.inputIntervalTicks, 1, 30) ||
            !number(f[7], entry.thinkTicks, 0, 180) ||
            !number(f[8], entry.minPieceTicks, 1, 600) || !ids.insert(f[0]).second) {
            std::fprintf(stderr, "[opponents] invalid or duplicate character: %s\n", f[0].c_str());
            continue;
        }
        entry.id=f[0]; entry.name=f[1]; entry.path=key(std::filesystem::u8path(f[2]));
        entry.iconPath=f[3]; entry.portraitPath=f[4];
        if (!f[5].empty()) entry.difficulty=f[5];
        configuredModels.insert(entry.path);
        roster.push_back(std::move(entry));
    }
    if (roster.empty()) {
        Opponent entry;
        entry.name="Practice Partner"; entry.path="@heuristic"; entry.id="heuristic";
        entry.difficulty="Easy";
        roster.push_back(entry);
    }

    // Backward-compatible path|name|interval[|think|min_piece] for auto-discovered models.
    std::unordered_map<std::string, std::vector<std::string>> overrides;
    std::ifstream old(legacy);
    while (std::getline(old, line)) {
        auto f=fields(line);
        if (!f[0].empty()) overrides[f[0]]=std::move(f);
    }
    namespace fs=std::filesystem;
    std::vector<Opponent> extra;
    for (const char* dir : {"model", "model/bots"}) {
        std::error_code ec;
        for (fs::directory_iterator it(dir, ec), end; !ec && it!=end; it.increment(ec)) {
            if (!it->is_regular_file(ec) || it->path().extension()!=".onnx") continue;
            auto path=key(it->path());
            if (!configuredModels.insert(path).second) continue;
            Opponent entry;
            entry.path=path; entry.id=path; entry.name=it->path().stem().u8string();
            std::replace(entry.name.begin(),entry.name.end(),'_',' ');
            extra.push_back(std::move(entry));
        }
    }
    std::sort(extra.begin(),extra.end(),[](const Opponent& a,const Opponent& b){return a.path<b.path;});
    roster.insert(roster.end(),extra.begin(),extra.end());
    for (auto& entry:roster) {
        // Explicit character profiles own their settings; legacy overrides apply to
        // auto discovery and the fallback partner only.
        if (ids.count(entry.id)) continue;
        auto it=overrides.find(entry.path);
        if (it==overrides.end()) it=overrides.find(fs::u8path(entry.path).filename().u8string());
        if (it==overrides.end()) continue;
        const auto& f=it->second;
        if (f.size()>1 && !f[1].empty()) entry.name=f[1];
        if (f.size()>2) number(f[2],entry.inputIntervalTicks,1,30);
        if (f.size()>3) number(f[3],entry.thinkTicks,0,180);
        if (f.size()>4) number(f[4],entry.minPieceTicks,1,600);
    }
    return roster;
}
}
