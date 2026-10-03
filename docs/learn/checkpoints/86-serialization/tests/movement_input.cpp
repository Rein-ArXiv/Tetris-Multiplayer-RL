#include "platform/platform.h"
#include "simulation/catalog.h"
#include "simulation/movement.h"
#include <cstdio>
int main() {
    if(!platform_init(640,480,"scripted movement"))return 1;
    auto piece=*study_catalog::make_piece(study_catalog::Kind::T);
    int moved=0,frames=0;
    while(!platform_should_close()) {
        platform_begin_frame();
        if(platform_should_close()||platform_key_pressed(Key::Escape))break;
        const auto result=study_movement::try_shift(piece,study_movement::horizontal_intent(
            platform_key_pressed(Key::Left),platform_key_pressed(Key::Right)));
        if(result==study_movement::Result::invalid)return 1;
        if(result==study_movement::Result::moved)++moved;
        ++frames;
        platform_end_frame();
    }
    platform_shutdown();
    if(moved!=1||frames!=4||piece.origin.column!=2)return 1;
    std::puts("scripted input: press moves once, held/released frames do not repeat");
}
