#pragma once
#include "renderer/batch_scene.h"
#include "renderer/flush_stream.h"
#include "presentation/game_view.h"
#include "renderer/board_geometry.h"
#include "renderer/piece_geometry.h"
#include "renderer/next_preview.h"
#include "renderer/end_marker.h"
#include "renderer/score_view.h"

namespace study_flush {
using namespace study_batch;
template<class Sink>
bool menu(Stream<Sink>& stream) noexcept {
    constexpr study_mesh::Vertex2 triangle[]={{-.25f,-.4f},{.35f,0},{-.25f,.4f}};
    return stream.append(triangle,3,filled_color);
}
// All geometry keeps its original order. Earlier flushed chunks are irreversible.
template<class Sink>
bool scene(Stream<Sink>& stream, const study_game::GameView& view,
           Clip full, Clip board_clip) noexcept {
    if (!stream.set_clip(board_clip)) return false;
    const auto board=study_board::make_mesh(view.board);
    if (!stream.append(board.vertices.data(),board.empty_vertices,empty_color) ||
        !stream.append(board.vertices.data()+board.empty_vertices,
                          board.vertices.size()-board.empty_vertices,filled_color)) return false;
    if (view.active && !view.ghost) return false;
    if (view.ghost) {
        const auto cells=study_piece::to_board(view.ghost->piece);
        if (!cells) return false;
        const auto mesh=study_piece_view::make_visible_mesh(*cells);
        if (!stream.append(mesh.vertices.data(),mesh.count,ghost_color)) return false;
    }
    if (view.active) {
        const auto cells=study_piece::to_board(*view.active);
        if (!cells) return false;
        const auto mesh=study_piece_view::make_visible_mesh(*cells);
        if (!stream.append(mesh.vertices.data(),mesh.count,piece_color)) return false;
    }
    if (!stream.set_clip(full)) return false;
    const auto next=study_next_view::make_mesh(view.next);
    if (!next || !stream.append(next->vertices.data(),next->count,piece_color)) return false;
    if (view.end_reason != study_round::EndReason::none) {
        const auto end=study_end_view::make_mesh();
        if (!stream.append(end.data(),end.size(),end_color)) return false;
    }
    const auto score=study_score_view::make_mesh(view.score);
    if (!stream.append(score.vertices.data(),score.count,score_color)) return false;
    return true;
}
} // namespace study_flush
