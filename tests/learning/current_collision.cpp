#include "src/sim_blocks.h"
#include "src/sim_grid.h"
#include "simulation/catalog.h"
#include "simulation/collision.h"
#include <array>
#include <cstdio>
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"current collision line %d\n",__LINE__);return 1;}}while(false)
int main() {
    std::array<SimBlock,7> blocks{{SimIBlock(),SimJBlock(),SimLBlock(),SimOBlock(),SimSBlock(),SimTBlock(),SimZBlock()}};
    int checks=0;
    for(int obstacle=-1;obstacle<200;++obstacle) {
        SimGrid actual;study_grid::Grid teaching;
        if(obstacle>=0) {
            actual.grid[obstacle/10][obstacle%10]=9;
            CHECK(teaching.set(obstacle/10,obstacle%10,study_grid::Cell::filled));
        }
        for(auto& block:blocks) for(int row:{-1,0,18,19}) for(int column:{-1,0,3,7,9,10}) {
            block.rowOffset=row;block.columnOffset=column;
            const auto* definition=study_catalog::find_id(block.id);CHECK(definition);
            auto piece=*study_catalog::make_piece(definition->kind);piece.origin={row,column};
            bool fits=true;
            for(const auto cell:block.GetCellPositions())if(!actual.IsCellEmpty(cell.row,cell.column))fits=false;
            CHECK(fits==(study_collision::classify(teaching,piece)==study_collision::Placement::clear));
            ++checks;
        }
    }
    SimGrid grid;
    for(int id=0;id<=9;++id) {
        grid.grid[0][0]=id;
        CHECK(grid.IsCellEmpty(0,0)==(id==0 || id==8));
    }
    CHECK(!grid.IsCellEmpty(-1,0) && !grid.IsCellEmpty(0,10) && !grid.IsCellEmpty(20,0));
    std::printf("Current cell occupancy and block coordinates: %d placements matched; ID8 compatibility and boundaries distinguished\n",checks);
}
