#pragma once
#include "renderer/color_batch.h"
#include "presentation/game_view.h"
#include "renderer/board_geometry.h"
#include "renderer/piece_geometry.h"
#include "renderer/next_preview.h"
#include "renderer/end_marker.h"
#include "renderer/score_view.h"

namespace study_batch {
inline constexpr Color empty_color{.125f,.25f,.375f,1};
inline constexpr Color filled_color{1,.5f,0,1};
inline constexpr Color piece_color{0,.75f,1,1};
inline constexpr Color ghost_color{.5f,.5f,.5f,1};
inline constexpr Color end_color{1,.125f,.125f,1};
inline constexpr Color score_color{.5f,1,.25f,1};
// Worst case: board + ghost + active + three previews + end + 20 digits.
static_assert(Batch::capacity >= 1200 + 24 + 24 + 72 + 12 + 840);
inline bool menu(Batch& output) noexcept {
    constexpr study_mesh::Vertex2 triangle[]={{-.25f,-.4f},{.35f,0},{-.25f,.4f}};
    Batch candidate;
    if (!candidate.append(triangle,3,filled_color)) return false;
    output = candidate;
    return true;
}
// Snapshot in, ordered coloured triangles out. Publish only a complete frame.
inline bool scene(Batch& output, const study_game::GameView& view) noexcept {
    Batch candidate;
    const auto board=study_board::make_mesh(view.board);
    if (!candidate.append(board.vertices.data(),board.empty_vertices,empty_color) ||
        !candidate.append(board.vertices.data()+board.empty_vertices,
                          board.vertices.size()-board.empty_vertices,filled_color)) return false;
    if (view.active && !view.ghost) return false;
    if (view.ghost) {
        const auto cells=study_piece::to_board(view.ghost->piece);
        if (!cells) return false;
        const auto mesh=study_piece_view::make_visible_mesh(*cells);
        if (!candidate.append(mesh.vertices.data(),mesh.count,ghost_color)) return false;
    }
    if (view.active) {
        const auto cells=study_piece::to_board(*view.active);
        if (!cells) return false;
        const auto mesh=study_piece_view::make_visible_mesh(*cells);
        if (!candidate.append(mesh.vertices.data(),mesh.count,piece_color)) return false;
    }
    const auto next=study_next_view::make_mesh(view.next);
    if (!next || !candidate.append(next->vertices.data(),next->count,piece_color)) return false;
    if (view.end_reason != study_round::EndReason::none) {
        const auto end=study_end_view::make_mesh();
        if (!candidate.append(end.data(),end.size(),end_color)) return false;
    }
    const auto score=study_score_view::make_mesh(view.score);
    if (!candidate.append(score.vertices.data(),score.count,score_color)) return false;
    output = candidate;
    return true;
}
} // namespace study_batch
