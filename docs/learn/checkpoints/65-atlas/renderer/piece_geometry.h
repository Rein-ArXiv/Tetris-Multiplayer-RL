#pragma once
#include "board_geometry.h"
#include "simulation/piece.h"
namespace study_piece_view {
struct Mesh {
    std::array<study_mesh::Vertex2,24> vertices{};
    std::size_t count=0; // Used prefix only; invisible cells do not submit zeros.
};
// Visual filtering only: out-of-board cells are omitted, not clamped or moved.
// This function knows no Grid occupancy and is NOT collision validation.
inline Mesh make_visible_mesh(const study_piece::BoardCells& cells) noexcept {
    Mesh mesh{};
    for(const auto cell:cells){
        const auto rect=study_board::cell_rect(cell.row,cell.column);
        if(!rect)continue;
        for(const auto vertex:study_board::quad_vertices(*rect))
            mesh.vertices[mesh.count++]=vertex;
    }
    return mesh;
}
} // namespace study_piece_view
