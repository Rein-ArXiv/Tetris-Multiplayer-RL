#include "simulation/grid.h"
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <type_traits>
#include <utility>
using study_grid::Grid;
using study_grid::Cell;
using study_grid::Position;
#define CHECK(...) do { if(!(__VA_ARGS__)) {std::fprintf(stderr,"grid line %d\n",__LINE__);std::exit(1);} } while(false)
static_assert(Grid::kCount==200);
static_assert(Grid::index_of(1,2)==12);
static_assert(!Grid::index_of(0,10));
static_assert(Grid{}.get(0,0)==Cell::empty);
static_assert(std::is_same<decltype(std::declval<const Grid&>().cells()),
                           const std::array<Cell,Grid::kCount>&>::value);
int main() {
    Grid board;
    for(auto cell:board.cells())CHECK(cell==Cell::empty);
    CHECK(board.get(0,0).has_value() && *board.get(0,0)==Cell::empty);
    CHECK(!board.get(-1,0).has_value() && !board.is_empty(-1,0));
    std::array<bool,Grid::kCount> seen{};
    for(int row=0;row<Grid::kRows;++row) for(int col=0;col<Grid::kColumns;++col) {
        const auto index=Grid::index_of(row,col);CHECK(index && *index==std::size_t(row*10+col));
        CHECK(!seen[*index]);seen[*index]=true;
        const auto p=Grid::position_of(*index);CHECK(p&&p->row==row&&p->column==col);
        CHECK(board.set(row,col,Cell::filled));
        CHECK(board.get(row,col)==Cell::filled&&!board.is_empty(row,col));
        CHECK(board.cells()[*index]==Cell::filled);
    }
    for(bool hit:seen)CHECK(hit);
    const auto before=board.cells(); // copy, not a borrowed view
    const int lo=std::numeric_limits<int>::min(),hi=std::numeric_limits<int>::max();
    for(Position bad : {Position{-1,0},Position{0,-1},Position{20,0},Position{0,10},
                       Position{1,-1},Position{-1,10},Position{lo,lo},Position{hi,hi},
                       Position{lo,0},Position{0,lo},Position{hi,0},Position{0,hi}}) {
        CHECK(!Grid::contains(bad.row,bad.column));
        CHECK(!Grid::index_of(bad.row,bad.column));
        CHECK(!board.get(bad.row,bad.column));
        CHECK(!board.set(bad.row,bad.column,Cell::empty));
        CHECK(!board.is_empty(bad.row,bad.column));
        CHECK(board.cells()==before);
    }
    CHECK(!Grid::position_of(200));
    CHECK(!Grid::position_of(std::numeric_limits<std::size_t>::max()));
    CHECK(!board.set(0,0,static_cast<Cell>(2)) && board.cells()==before);
    CHECK(!board.set(19,9,static_cast<Cell>(255)) && board.cells()==before);
    Grid copy=board;
    const auto& view=board.cells();
    const auto snapshot=board.cells();
    CHECK(board.set(0,0,Cell::empty));
    CHECK(view[0]==Cell::empty && snapshot[0]==Cell::filled && copy.get(0,0)==Cell::filled);
    copy.clear();
    for(auto cell:copy.cells())CHECK(cell==Cell::empty);
    CHECK(board.get(19,9)==Cell::filled);
    std::puts("Grid: all 200 bijections, signed bounds, alias rejection, invalid values, unchanged state on failure, copies/views passed");
}
