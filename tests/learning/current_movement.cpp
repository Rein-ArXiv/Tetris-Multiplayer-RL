#include "src/sim_game.h"
#include "simulation/catalog.h"
#include "simulation/movement.h"
#include <cstdio>
#define CHECK(x) do { if (!(x)) { std::fprintf(stderr,"current movement line %d\n",__LINE__);return 1; } } while(false)
int main() {
    for (int seed = 1; seed <= 32; ++seed) {
        SimGame game(seed);
        const auto* definition = study_catalog::find_id(game.CurrentBlockId());
        CHECK(definition);
        auto piece = *study_catalog::make_piece(definition->kind);
        for (int step : {-1, -1, -1, -1, -1, -1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, -1}) {
            CHECK(study_movement::try_shift(piece,step) != study_movement::Result::invalid);
            game.SubmitInput(step < 0 ? INPUT_LEFT : INPUT_RIGHT);
            CHECK(game.CurrentCol()==piece.origin.column && game.CurrentRow()==piece.origin.row);
            const auto expected = study_piece::to_board(piece);
            const auto actual = game.CurrentBlock().GetCellPositions();
            CHECK(expected && actual.size()==expected->size());
            for (std::size_t i=0;i<actual.size();++i) {
                CHECK(actual[i].row==(*expected)[i].row && actual[i].column==(*expected)[i].column);
            }
        }
        for(int n=0;n<12;++n)game.SubmitInput(INPUT_LEFT);
        CHECK(game.CurrentCol()==0);
        game.SubmitInput(INPUT_LEFT | INPUT_RIGHT);
        CHECK(game.CurrentCol()==1); // Production executes left then right; not cancellation.
    }
    std::puts("Current movement: 32 seeds, empty-board single directions match; simultaneous bits intentionally differ at wall");
}
