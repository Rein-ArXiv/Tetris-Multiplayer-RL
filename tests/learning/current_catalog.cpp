#include "src/sim_blocks.h"
#include "simulation/catalog.h"
#include <array>
#include <cstdio>
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"current catalog line %d: %s\n",__LINE__,#x);return 1;}}while(false)
int main(){
    const std::array<SimBlock,7> current{{SimIBlock(),SimJBlock(),SimLBlock(),SimOBlock(),SimSBlock(),SimTBlock(),SimZBlock()}};
    for(std::size_t i=0;i<current.size();++i){
        const auto& ref=current[i];const auto& definition=study_catalog::definitions[i];
        CHECK(ref.id==static_cast<int>(definition.kind));
        CHECK(ref.rowOffset==definition.spawn.row&&ref.columnOffset==definition.spawn.column);
        const auto piece=study_catalog::make_piece(definition.kind);CHECK(piece);
        const auto cells=study_piece::to_board(*piece);CHECK(cells);
        const auto actual=ref.GetCellPositions();CHECK(actual.size()==cells->size());
        for(std::size_t c=0;c<4;++c)CHECK(actual[c].row==(*cells)[c].row&&actual[c].column==(*cells)[c].column);
    }
    std::puts("Current seven spawn definitions: IDs, ordered cells and origins match teaching catalog");
}
