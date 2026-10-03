#include "src/sim_grid.h"
#include <cstdio>
#include <cstdlib>
#include <limits>
#define CHECK(...) do{if(!(__VA_ARGS__)){std::fprintf(stderr,"current grid line %d\n",__LINE__);std::exit(1);}}while(false)
int main(){
    SimGrid grid;
    for(int r=0;r<SimGrid::kRows;++r) for(int c=0;c<SimGrid::kCols;++c)CHECK(grid.IsCellEmpty(r,c));
    for(int id=0;id<=9;++id){grid.grid[1][2]=id;CHECK(grid.IsCellEmpty(1,2)==(id==0||id==8));}
    CHECK(grid.IsCellOutside(-1,0)&&!grid.IsCellEmpty(-1,0));
    CHECK(grid.IsCellOutside(0,10)&&!grid.IsCellEmpty(0,10));
    CHECK(grid.IsCellOutside(20,0)&&!grid.IsCellEmpty(20,0));
    CHECK(!grid.IsCellEmpty(std::numeric_limits<int>::max(),std::numeric_limits<int>::min()));
    grid.grid[19][9]=9;SimGrid copy=grid;copy.Initialize();
    CHECK(grid.grid[19][9]==9&&copy.grid[19][9]==0);
    std::printf("Current SimGrid: bounds, IDs0/8 vs1..7/9 and value-copy passed; cells=%zu bytes sizeof(int)=%zu\n",sizeof(grid.grid),sizeof(int));
}
