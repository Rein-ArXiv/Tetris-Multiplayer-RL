#include "renderer/flush_scene.h"
#include "client/game.h"
#include <cstdio>
struct ConsoleSink {
 bool set_clip(study_flush::Clip c) noexcept {std::printf("clip (%d,%d %dx%d): ",c.x,c.y,c.width,c.height);return true;}
 bool submit(const study_batch::Batch& b) noexcept {std::printf("%zu vertices\n",b.size());return true;}
};
int main(){
 auto round=study_round::Round::create_seeded(study_grid::Grid{},1);if(!round)return 1;
 study_game::Game game(*round);auto view=game.view();if(!view)return 1;
 ConsoleSink sink;const study_flush::Clip full{0,0,320,240},board{110,20,100,200};
 study_flush::Stream<ConsoleSink> stream(sink,full);
 if(!study_flush::scene(stream,*view,full,board)||!stream.finish())return 1;
 const auto stats=stream.statistics();
 std::printf("%llu vertices in %llu ordered submissions\n",static_cast<unsigned long long>(stats.vertices),static_cast<unsigned long long>(stats.draws));
}
