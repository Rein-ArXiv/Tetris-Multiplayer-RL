#include "platform/platform.h"
#include "simulation/catalog.h"
#include "simulation/collision.h"
#include "src/board_example.h"
#include <cstdio>
int main() {
    if(!platform_init(640,480,"scripted collision"))return 1;
    const auto board=make_example_board();const auto before=board.cells();
    auto piece=*study_catalog::make_piece(study_catalog::Kind::T);
    int blocked=0,idle=0;
    while(!platform_should_close()) {
        platform_begin_frame();
        if(platform_should_close() || platform_key_pressed(Key::Escape))break;
        const auto result=study_collision::try_shift(board,piece,study_movement::horizontal_intent(
            platform_key_pressed(Key::Left),platform_key_pressed(Key::Right)));
        if(result==study_movement::Result::blocked)++blocked;
        else if(result==study_movement::Result::idle)++idle;
        else return 1;
        platform_end_frame();
    }
    platform_shutdown();
    if(blocked!=1 || idle!=3 || piece.origin.column!=3 || board.cells()!=before)return 1;
    std::puts("scripted collision: press rejected once; held/released idle, piece and board preserved");
}
