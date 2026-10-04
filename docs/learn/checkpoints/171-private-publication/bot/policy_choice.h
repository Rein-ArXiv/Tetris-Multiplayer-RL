#ifndef BOT_CHOOSE_FINITE_LEGAL_H
#define BOT_CHOOSE_FINITE_LEGAL_H

#include <climits>
#include <cmath>
#include <cstddef>

namespace bot {

// Preconditions: if count > 0, scores and legal each point to count valid
// elements. Does not own them and never writes through them. No allocation.
// Effect: on success writes the index of the first legal maximum score to
// action and returns true. On any failure returns false and leaves action
// unchanged.
inline bool choose_finite_legal(const float* scores, const bool* legal,
                                std::size_t count, int& action) noexcept {
    if (scores == nullptr || legal == nullptr) return false;
    if (count == 0) return false;
    if (count > static_cast<std::size_t>(INT_MAX)) return false;

    // Every score must be finite, even entries that are not legal.
    for (std::size_t i = 0; i < count; ++i) {
        if (!std::isfinite(scores[i])) return false;
    }

    bool found = false;
    float best = 0.0f;
    std::size_t bestIndex = 0;
    for (std::size_t i = 0; i < count; ++i) {
        if (!legal[i]) continue;
        if (!found || scores[i] > best) {
            found = true;
            best = scores[i];
            bestIndex = i;
        }
    }
    if (!found) return false;

    action = static_cast<int>(bestIndex);
    return true;
}

} // namespace bot

#endif // BOT_CHOOSE_FINITE_LEGAL_H
