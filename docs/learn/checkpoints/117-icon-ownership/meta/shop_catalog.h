#pragma once
// Shop icon catalog + strict syntax-only parsing of a chosen icon id.
// The parser checks syntax; shop_icon supplies the server catalog policy.

#include <array>
#include <optional>
#include <string>

#include "meta/wire.h"

namespace study_meta {

struct ShopIcon {
    const char* id;
    const char* label;
    int price;
};

inline constexpr std::array<ShopIcon, 3> kShopIcons{{
    {"default", "Default", 0},
    {"ruby", "Ruby", 100},
    {"gold", "Gold", 250},
}};

// Exact, NUL-safe lookup; nullptr when absent.
inline const ShopIcon* shop_icon(const std::string& id) {
    for (const ShopIcon& icon : kShopIcons) {
        if (id == icon.id) {
            return &icon;
        }
    }
    return nullptr;
}

// Strict JSON { "icon_id": "<id>" }; syntax only, caller checks the catalog.
inline std::optional<std::string> parse_icon_choice(const std::string& body) {
    auto obj = object(body, 1);
    if (!obj) {
        return std::nullopt;
    }
    auto it = obj->find("icon_id");
    if (it == obj->end() || !it->is_string()) {
        return std::nullopt;
    }

    const std::string value = it->get<std::string>();
    if (value.empty() || value.size() > 32) {
        return std::nullopt;
    }
    for (unsigned char c : value) {
        const bool ok = (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
                        c == '_' || c == '-';
        if (!ok) {
            return std::nullopt;
        }
    }
    return value;
}

}  // namespace study_meta
