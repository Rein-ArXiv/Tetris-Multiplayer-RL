#include "board_example.h"
#include "renderer/board_geometry.h"
#include <cstdio>
int main() {
    const auto board=make_example_board();
    const auto mesh=study_board::make_mesh(board);
    for (const study_grid::Position p : {study_grid::Position{0,0},{1,2},{19,9}}) {
        const auto r=study_board::cell_rect(p.row,p.column);
        std::printf("(%d,%d) -> [%.0f,%.0f)-[%.0f,%.0f)\n",
                    p.row,p.column,r->left,r->top,r->right,r->bottom);
    }
    std::printf("vertices: empty=%zu filled=%zu total=%zu bytes=%zu\n",
                mesh.empty_vertices,study_board::vertex_count-mesh.empty_vertices,
                mesh.vertices.size(),sizeof(mesh.vertices));
    const auto gap=study_board::cell_at(139.5,39.5);
    if(!gap)return 1;
    std::printf("gap (139.5,39.5) -> row=%d column=%d\n",gap->row,gap->column);
}
