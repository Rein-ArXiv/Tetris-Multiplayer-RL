#pragma once
#include "simulation/catalog.h"
#include <array>
#include <optional>
#include <cstddef>
namespace study_next {
// Deterministic exercise input, not a shuffled bag or a random generator.
// Owns a copy of the pattern. Every returned Kind is independently copied.
class ScriptedSource {
public:
    static std::optional<ScriptedSource> from_pattern(
        const std::array<study_catalog::Kind,7>& pattern, std::size_t count) noexcept {
        if(count==0||count>pattern.size())return std::nullopt;
        for(std::size_t i=0;i<count;++i)if(!study_catalog::find(pattern[i]))return std::nullopt;
        ScriptedSource source;source.pattern_=pattern;source.count_=count;return source;
    }
    static std::optional<ScriptedSource> cycle(study_catalog::Kind first) noexcept {
        std::size_t start=0;
        while(start<7&&study_catalog::definitions[start].kind!=first)++start;
        if(start==7)return std::nullopt;
        std::array<study_catalog::Kind,7> pattern{};
        for(std::size_t i=0;i<7;++i)pattern[i]=study_catalog::definitions[(start+i)%7].kind;
        return from_pattern(pattern,7);
    }
    static std::optional<ScriptedSource> repeating(study_catalog::Kind kind) noexcept {
        std::array<study_catalog::Kind,7> pattern{};pattern[0]=kind;
        return from_pattern(pattern,1);
    }
    study_catalog::Kind next() noexcept {
        const auto value=pattern_[cursor_];
        cursor_=(cursor_+1)%count_;
        return value;
    }
    std::size_t cursor() const noexcept { return cursor_; }
private:
    ScriptedSource() = default;
    std::array<study_catalog::Kind,7> pattern_{};
    std::size_t count_=1;
    std::size_t cursor_=0;
};
} // namespace study_next
