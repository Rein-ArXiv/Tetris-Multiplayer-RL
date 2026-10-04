#pragma once
#include "bindings/session.h"
#include <cstdlib>

namespace study_practice {
// Deterministic local heuristic for this checkpoint, not a trained model.
// Weights are fixture policy, not universal difficulty or the product's exact policy.
inline bool pick(const study_python::Session& observed, int& action) {
    bool found = false;
    long best = 0;
    for (int candidate : observed.legal_actions()) {
        auto trial = observed.clone();
        const auto result = trial.apply_action(candidate);
        const auto grid = trial.grid();
        const int rows = static_cast<int>(grid.size());
        const int columns = static_cast<int>(grid.front().size());
        long height = 0, holes = 0, bump = 0;
        int previous = 0;
        for (int column = 0; column < columns; ++column) {
            bool filled = false;
            int columnHeight = 0;
            for (int row = 0; row < rows; ++row) {
                if (grid[row][column]) {
                    if (!filled) { columnHeight = rows - row; height += columnHeight; }
                    filled = true;
                } else if (filled) { ++holes; }
            }
            if (column) bump += std::abs(columnHeight - previous);
            previous = columnHeight;
        }
        const long score = result.lines * 20L - height * 2 - holes * 8 - bump;
        if (!found || score > best) { found = true; best = score; action = candidate; }
    }
    return found;
}
}
