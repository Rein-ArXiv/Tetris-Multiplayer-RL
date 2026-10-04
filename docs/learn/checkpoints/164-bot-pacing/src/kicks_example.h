#pragma once
#include "simulation/kicks.h"
namespace kicks_example {
struct Example {
    study_grid::Grid board;
    study_catalog::Kind kind = study_catalog::Kind::T;
    int quarter = 0;
    study_piece::Origin origin{5,3};
};
// Seven fixed inputs for the CPU teaching demo, not the application's game rules.
inline Example make(int number) {
    Example example;
    if(number==0){example.quarter=1;example.origin={5,-1};}
    if(number==1||number==2)example.origin={18,3};
    if(number==2)example.kind=study_catalog::Kind::I;
    if(number==3||number==4)(void)example.board.set(7,4,study_grid::Cell::filled);
    if(number==4)(void)example.board.set(7,3,study_grid::Cell::filled);
    if(number==5){
        for(int row=0;row<20;++row)for(int column=0;column<10;++column)
            (void)example.board.set(row,column,study_grid::Cell::filled);
        for(auto cell:{study_grid::Position{5,4},{6,3},{6,4},{6,5}})
            (void)example.board.set(cell.row,cell.column,study_grid::Cell::empty);
    }
    if(number==6)example.kind=study_catalog::Kind::O;
    return example;
}
} // namespace kicks_example
