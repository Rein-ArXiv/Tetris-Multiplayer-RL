#include "simulation/catalog.h"
#include "simulation/movement.h"
#include <cstdio>

int main() {
    auto piece = *study_catalog::make_piece(study_catalog::Kind::T);
    const int directions[] = {-1, -1, -1, -1, 0, 1};
    for (int direction : directions) {
        const int before = piece.origin.column;
        const auto result = study_movement::try_shift(piece, direction);
        const char* name = result == study_movement::Result::moved ? "moved" :
                           result == study_movement::Result::blocked ? "blocked" :
                           result == study_movement::Result::idle ? "idle" : "invalid";
        std::printf("step=%d column=%d -> %d result=%s\n",
                    direction, before, piece.origin.column, name);
    }
}
