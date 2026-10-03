#include "renderer/batch_scene.h"
#include "client/game.h"
#include <cstdio>
int main(){
    const auto round=study_round::Round::create_seeded(study_grid::Grid{},1);
    if(!round)return 1;
    study_game::Game game(*round);const auto view=game.view();study_batch::Batch batch;
    if(!view||!study_batch::scene(batch,*view))return 1;
    std::printf("initial scene: %zu vertices x %zu bytes = %zu bytes\n",
        batch.size(),sizeof(study_batch::Vertex),batch.size()*sizeof(study_batch::Vertex));
    std::puts("one compatible opaque stream; Device submits one draw (CPU demo makes no GL calls)");
    if(!study_batch::menu(batch))return 1;
    std::printf("menu: %zu vertices; capacity=%zu, used prefix only\n",batch.size(),batch.capacity);
}
