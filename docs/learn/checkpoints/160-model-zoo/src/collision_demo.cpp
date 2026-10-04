#include "simulation/catalog.h"
#include "simulation/collision.h"
#include "board_example.h"
#include <cstdio>

static const char* name(study_collision::Placement value) {
    switch (value) {
    case study_collision::Placement::clear: return "clear";
    case study_collision::Placement::outside: return "outside";
    case study_collision::Placement::occupied: return "occupied";
    case study_collision::Placement::unrepresentable: return "unrepresentable";
    }
    return "unknown";
}
int main() {
    const auto board = make_example_board();
    const auto initial = *study_catalog::make_piece(study_catalog::Kind::T);
    for (int column : {3, 2, -1, 8}) {
        auto candidate = initial;
        candidate.origin.column = column;
        std::printf("placement column=%d: %s\n", column,
                    name(study_collision::classify(board, candidate)));
    }
    auto piece = initial;
    for (int direction : {-1, 0, 1, -1, -1}) {
        const int before = piece.origin.column;
        const auto result = study_collision::try_shift(board, piece, direction);
        const char* text = result == study_movement::Result::moved ? "moved" :
                           result == study_movement::Result::blocked ? "blocked" :
                           result == study_movement::Result::idle ? "idle" : "invalid";
        std::printf("step=%d column=%d -> %d result=%s\n",
                    direction, before, piece.origin.column, text);
    }
}
