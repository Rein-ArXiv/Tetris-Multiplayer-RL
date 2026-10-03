#include "simulation/round.h"
#include <cstdio>
int main() {
    study_grid::Grid board;
    if (!board.set(10,4,study_grid::Cell::filled)) return 1;
    const auto round=study_round::Round::create(board,study_catalog::Kind::T);
    if (!round || !round->active()) return 1;
    for (int n=0;n<3;++n) {
        const auto ghost=round->ghost();
        if (!ghost) return 1;
        std::printf("active=%d ghost=%d distance=%d score=%llu elapsed=%d\n",
            round->active()->origin.row,ghost->piece.origin.row,ghost->distance,
            static_cast<unsigned long long>(round->score()),round->gravity().elapsed);
    }
}
