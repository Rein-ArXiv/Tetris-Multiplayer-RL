#include "src/sim_blocks.h"
#include <cstdio>
#include <stdexcept>
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"current piece line %d: %s\n",__LINE__,#x);return 1;}}while(false)
int main(){
    SimTBlock original;auto before=original.GetCellPositions();
    CHECK(before.size()==4&&before[0].row==0&&before[0].column==4);
    original.Move(5,0);const auto after=original.GetCellPositions();
    CHECK(after[0].row==5&&after[0].column==4&&before[0].row==0);
    before[0].row=77;CHECK(original.cells.at(0).at(0).row==0);
    SimBlock copy=original;copy.id=8;copy.Move(3,0);copy.cells.at(0).at(0).column=99;
    CHECK(original.id==6&&original.rowOffset==5&&original.cells.at(0).at(0).column==1);
    CHECK(copy.columnOffset==original.columnOffset&&copy.rotationState==original.rotationState);
    SimOBlock o;CHECK(o.columnOffset==4);CHECK(o.GetCellPositions()[0].column==4);
    SimBlock uninitialized;bool rejected=false;
    try{(void)uninitialized.GetCellPositions();}catch(const std::out_of_range&){rejected=true;}
    CHECK(rejected);
    std::puts("current SimBlock: board coordinates, independent map/vector copy, snapshots and required shape data passed");
}
