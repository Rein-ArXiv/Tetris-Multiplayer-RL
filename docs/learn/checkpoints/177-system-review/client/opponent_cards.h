#pragma once
#include "bot/characters.h"
#include <string>
#include <vector>

namespace study_opponent_menu {
// Owned presentation values: a renderer may cache these while a roster is replaced.
struct Card {
    std::string id, name, icon, portrait, difficulty;
};
inline Card card(const study_characters::Character& selected) {
    const auto& appearance = selected.appearance;
    return {selected.id, appearance.name, appearance.icon, appearance.portrait,
            appearance.difficulty};
}
// Build once when the roster changes, not once per rendered frame.
inline std::vector<Card> cards(const study_characters::Catalog& catalog) {
    std::vector<Card> result;
    result.reserve(catalog.entries().size());
    for (const auto& entry : catalog.entries()) result.push_back(card(entry));
    return result;
}
}
