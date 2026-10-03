#pragma once
#include "simulation/catalog.h"
#include "simulation/grid.h"
#include "src/clear_example.h"
#include <optional>
namespace spawn_example {
enum class Scenario { normal, initial_blocked, next_blocked, clear_rescue };
inline std::optional<study_grid::Grid> make(study_catalog::Kind kind,Scenario scenario) noexcept {
    const auto piece=study_catalog::make_piece(kind);
    if(!piece)return std::nullopt;
    if(scenario==Scenario::normal)return make_clear_board(kind);
    const auto cells=study_piece::to_board(*piece);
    if(!cells)return std::nullopt;
    study_grid::Grid board;
    if(scenario==Scenario::initial_blocked){
        (void)board.set((*cells)[0].row,(*cells)[0].column,study_grid::Cell::filled);
        return board;
    }
    if(scenario!=Scenario::next_blocked&&scenario!=Scenario::clear_rescue)return std::nullopt;
    auto bottom=(*cells)[0];int top=bottom.row;
    for(auto cell:*cells){if(cell.row>bottom.row)bottom=cell;if(cell.row<top)top=cell.row;}
    if(!board.set(bottom.row+1,bottom.column,study_grid::Cell::filled))return std::nullopt;
    if(scenario==Scenario::clear_rescue){
        for(int row=top;row<=bottom.row;++row)for(int col=0;col<10;++col)
            (void)board.set(row,col,study_grid::Cell::filled);
        for(auto cell:*cells)(void)board.set(cell.row,cell.column,study_grid::Cell::empty);
    }
    return board;
}
} // namespace spawn_example
