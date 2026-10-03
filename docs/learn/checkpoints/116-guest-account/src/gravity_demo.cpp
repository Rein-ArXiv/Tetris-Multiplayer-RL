#include "simulation/gravity.h"
#include "simulation/catalog.h"
#include "src/gravity_example.h"
#include <cstdio>
int main() {
    const auto board=make_gravity_board();
    auto piece=*study_catalog::make_piece(study_catalog::Kind::T);
    study_gravity::Counter counter;
    for(int t=1;t<=300;++t) {
        const auto result=study_gravity::tick(board,piece,counter);
        if(result==study_gravity::TickResult::invalid)return 1;
        if(t%30==0)std::printf("tick=%d row=%d elapsed=%d %s\n",t,piece.origin.row,counter.elapsed,
            result==study_gravity::TickResult::moved?"moved":"blocked");
    }
}
