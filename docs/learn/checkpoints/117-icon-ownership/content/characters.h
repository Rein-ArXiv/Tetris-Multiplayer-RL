#pragma once
#include <array>
#include <cstddef>
#include <optional>
#include <string_view>

namespace study_characters {

struct Character {
    std::string_view id;
    const char* name;
    const char* icon_path;
    const char* portrait_path;
};

// Fixed compile-time catalog. Paths are UTF-8/ASCII literals; an empty path is
// allowed and means "no asset, use the caller's fallback".
inline constexpr std::array<Character, 2> characters{{
    {"player", u8"플레이어", "assets/player.png", "assets/player.png"},
    {"rook", u8"루크", "assets/bot.png", "assets/opponent.png"},
}};

inline constexpr std::string_view default_id="player";

// Compile-time sanity: ids non-empty and distinct, names non-null and
// non-empty, path pointers non-null (the path strings themselves may be empty).
constexpr bool catalog_valid() noexcept {
    bool default_found=false;
    for (std::size_t i = 0; i < characters.size(); ++i) {
        const Character& c = characters[i];
        if (c.id.empty()) return false;
        default_found |= c.id==default_id;
        if (c.name == nullptr || c.name[0] == '\0') return false;
        if (c.icon_path == nullptr) return false;
        if (c.portrait_path == nullptr) return false;
        for (std::size_t j = i + 1; j < characters.size(); ++j) {
            if (c.id == characters[j].id) return false;
        }
    }
    return default_found;
}

static_assert(catalog_valid(), "character catalog must be valid");

// Index of a known id, or nullopt when not present.
inline std::optional<std::size_t> find_index(std::string_view id) noexcept {
    for (std::size_t i = 0; i < characters.size(); ++i) {
        if (characters[i].id == id) return i;
    }
    return std::nullopt;
}

// Borrowed pointer into the static catalog, or nullptr when unknown.
inline const Character* find(std::string_view id) noexcept {
    const std::optional<std::size_t> index = find_index(id);
    return index ? &characters[*index] : nullptr;
}

} // namespace study_characters
