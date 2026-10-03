#include "simulation/round.h"
#include "src/clear_example.h"
#include <cstdio>
static int filled(const study_grid::Grid& board) {
    int count=0;for(auto cell:board.cells())if(cell==study_grid::Cell::filled)++count;
    return count;
}
int main() {
    const auto board=make_clear_board(study_catalog::Kind::O);
    auto round=study_round::Round::create(board,study_catalog::Kind::O);
    if(!round)return 1;
    std::printf("before: filled=%d\n",filled(board));
    for(int tick=1;tick<=600;++tick) {
        auto result=round->tick(0);
        if(result==study_round::Step::invalid)return 1;
        if(result==study_round::Step::locked) {
            std::printf("tick=%d cleared=%d filled=%d active_row=%d\n",tick,
                round->last_cleared(),filled(round->board()),round->active()->origin.row);
            std::printf("markers: (7,0)=%d (14,9)=%d\n",
                !round->board().is_empty(7,0),!round->board().is_empty(14,9));
            return 0;
        }
    }
    return 1;
}
