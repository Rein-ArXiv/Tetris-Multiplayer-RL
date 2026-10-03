#include "simulation/catalog.h"
#include "renderer/piece_geometry.h"
#include <cstdio>
int main(){
    std::size_t index=0;
    for(const auto& definition:study_catalog::definitions){
        const auto piece=study_catalog::make_piece(definition.kind);
        if(!piece)return 1;
        const auto cells=study_piece::to_board(*piece);if(!cells)return 1;
        const auto mesh=study_piece_view::make_visible_mesh(*cells);
        std::printf("index=%zu name=%.*s id=%d spawn=(%d,%d) visible=%zu\n",
            index++,static_cast<int>(definition.name.size()),definition.name.data(),
            static_cast<int>(definition.kind),piece->origin.row,piece->origin.column,mesh.count/6);
        for(int r=0;r<4;++r){
            for(int col=0;col<4;++col){
                bool filled=false;
                for(const auto cell:definition.cells)if(cell.row==r&&cell.column==col)filled=true;
                std::putchar(filled?'#':'.');
            }
            std::putchar('\n');
        }
    }
    std::printf("raw id 257: found=%d\n",study_catalog::find_id(257)!=nullptr);
}
