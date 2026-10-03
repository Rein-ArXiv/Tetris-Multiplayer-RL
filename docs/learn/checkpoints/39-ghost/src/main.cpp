#include "platform/platform.h"
#include "spawn_example.h"
#include "score_example.h"
#include "end_message.h"
#include "renderer/end_marker.h"
#include "renderer/score_view.h"
#include "renderer/ghost_view.h"
#include <cinttypes>
#include "simulation/round.h"
#include "simulation/pending_controls.h"
#include "timing/fixed_clock.h"
#include "simulation/catalog.h"
#include "simulation/movement.h"
#include "simulation/collision.h"
#include "renderer/piece_scene.h"
#include "renderer/next_preview.h"
#include "renderer/gl_api.h"
#include "renderer/mesh.h"
#include "renderer/vertex_buffer.h"
#include "renderer/vertex_array.h"
#include "renderer/shader.h"
#include "renderer/program.h"
#include "renderer/triangle.h"
#include "renderer/shader_sources.h"
#include "renderer/coordinates.h"
#include "renderer/quad.h"
#include "renderer/blend_scene.h"
#include "renderer/board_scene.h"
#include <cstdio>
#include <exception>
#include <string_view>

// The table and borrowed GL strings are used only while the context exists.
static int run_session(study_round::Round round)
{
    const auto& board = round.board();
    std::size_t occupied=0;
    for(const auto cell:board.cells()) if(cell==study_grid::Cell::filled) ++occupied;
    std::printf("board state: %d rows x %d columns, %zu occupied cells\n",
                study_grid::Grid::kRows,study_grid::Grid::kColumns,occupied);
    auto mesh = study_board::make_mesh(board);
    study_piece_view::Mesh piece_mesh;
    if(round.active()){
        const auto cells=study_piece::to_board(*round.active());
        if(!cells)return 1;
        piece_mesh=study_piece_view::make_visible_mesh(*cells);
    }
    if(round.finished())std::puts(study_ui::end_message(round.end_reason()));
    const auto swap = platform_set_swap_interval(1);
    std::printf("swap request=%d attempted=%d accepted=%d reported=%d (not measured refresh)\n",
                1, swap.attempted, swap.accepted, swap.reported_interval);
    study_gl::GlApi gl;
    if (!study_gl::load(gl, platform_gl_get_proc)) return 1;
    const auto* version = gl.GetString(study_gl::Version);
    const auto* renderer = gl.GetString(study_gl::Renderer);
    study_gl::GLint attributes = 0, major = 0, minor = 0, profile = 0;
    gl.GetIntegerv(study_gl::MajorVersion, &major);
    gl.GetIntegerv(study_gl::MinorVersion, &minor);
    gl.GetIntegerv(study_gl::ContextProfileMask, &profile);
    gl.GetIntegerv(study_gl::MaxVertexAttribs, &attributes);
    const bool version_ok = major > 3 || (major == 3 && minor >= 3);
    if (!version || !renderer || gl.GetError() != 0 || !version_ok ||
        !(profile & study_gl::CoreProfileBit)) {
        std::fputs("GL diagnostic query/version/profile failed\n", stderr);
        return 1;
    }
    std::printf("GL %s | %s | actual=%d.%d core, max attributes=%d\n",
                reinterpret_cast<const char*>(version), reinterpret_cast<const char*>(renderer),
                major, minor, attributes);

    // Declared after `gl`, destroyed before it and before platform_shutdown().
    study_gl::VertexBuffer vertex_buffer(gl);
    {
        if (!vertex_buffer.upload(mesh.vertices)) return 1;
    } // The GL data store owns a separate copy of the vertex positions.
    study_gl::GLint stored_bytes = 0;
    gl.GetBufferParameteriv(study_gl::ArrayBuffer, study_gl::BufferSize, &stored_bytes);
    if (gl.GetError() != 0 || stored_bytes != static_cast<study_gl::GLint>(sizeof(mesh.vertices))) return 1;
    std::printf("GL vertex buffer stores %d bytes (ready for draw)\n", stored_bytes);

    // A VAO borrows the buffer: destroy the VAO first, then the buffer.
    study_gl::VertexArray vertex_array(gl);
    if (!vertex_array.configure(vertex_buffer.name())) return 1;
    study_gl::GLint stride = 0, enabled = 0;
    gl.GetVertexAttribiv(0, study_gl::AttribStride, &stride);
    gl.GetVertexAttribiv(0, study_gl::AttribEnabled, &enabled);
    if (gl.GetError() != 0 || stride != sizeof(study_mesh::Vertex2) || enabled != 1) return 1;
    std::puts("VAO location 0: two floats, stride 8, offset 0, enabled (ready for draw)");

    study_gl::Program empty_program(gl), filled_program(gl);
    if (!study_board_scene::link_program(gl, empty_program, study_board_scene::empty_fragment) ||
        !study_board_scene::link_program(gl, filled_program, study_board_scene::filled_fragment)) return 1;
    std::printf("board mesh: %zu empty vertices + %zu filled vertices\n",
                mesh.empty_vertices, study_board::vertex_count - mesh.empty_vertices);
    if (!study_board_scene::configure(gl)) return 1;

    study_gl::VertexBuffer piece_buffer(gl);
    study_gl::VertexArray piece_array(gl);
    study_gl::Program piece_program(gl);
    if (piece_mesh.count > 0 &&
        (!piece_buffer.upload(piece_mesh.vertices.data(),piece_mesh.count) ||
         !piece_array.configure(piece_buffer.name()))) return 1;
    // The preview uses this program even when there is no active piece.
    if(!study_board_scene::link_program(gl,piece_program,study_piece_view::fragment))return 1;
    auto ghost = round.ghost();
    if (round.active() && !ghost) return 1;
    study_gl::VertexBuffer ghost_buffer(gl);
    study_gl::VertexArray ghost_array(gl);
    study_gl::Program ghost_program(gl);
    if (!study_board_scene::link_program(gl,ghost_program,study_ghost_view::fragment)) return 1;
    if (ghost) {
        const auto cells=study_piece::to_board(ghost->piece);
        if (!cells) return 1;
        const auto hint=study_piece_view::make_visible_mesh(*cells);
        if (!ghost_buffer.upload(hint.vertices.data(),hint.count) ||
            !ghost_array.configure(ghost_buffer.name())) return 1;
    }
    const auto end_mesh=study_end_view::make_mesh();
    study_gl::VertexBuffer end_buffer(gl);study_gl::VertexArray end_array(gl);
    study_gl::Program end_program(gl);
    if(!end_buffer.upload(end_mesh)||!end_array.configure(end_buffer.name())||
       !study_board_scene::link_program(gl,end_program,study_end_view::fragment))return 1;

    auto displayed_score = round.score();
    auto score_mesh = study_score_view::make_mesh(displayed_score);
    study_gl::VertexBuffer score_buffer(gl);study_gl::VertexArray score_array(gl);
    study_gl::Program score_program(gl);
    if (!score_buffer.upload(score_mesh.vertices) ||
        !score_array.configure(score_buffer.name()) ||
        !study_board_scene::link_program(gl,score_program,study_score_view::fragment)) return 1;
    study_timing::FixedClock clock;
    study_input::PendingControls pending;
    const auto preview=study_next_view::make_mesh(round.next());
    if(!preview||preview->count!=72)return 1;
    study_gl::VertexBuffer preview_buffer(gl);
    study_gl::VertexArray preview_array(gl);
    if(!preview_buffer.upload(preview->vertices.data(),preview->count)||
       !preview_array.configure(preview_buffer.name()))return 1;
    WindowSize previous_window{};
    DrawableSize previous_drawable{};
    double report_elapsed=0;
    while (!platform_should_close()) {
        const FrameInfo frame=platform_begin_frame();
        if (platform_should_close() || platform_key_pressed(Key::Escape)) break;
        pending.capture(platform_key_pressed(Key::Left), platform_key_pressed(Key::Right),
                        platform_key_pressed(Key::Up));
        const auto batch = clock.advance_seconds(frame.dt);
        if (!batch) return 1;
        bool piece_changed = false, board_changed = false;
        int cleared_this_frame = 0;
        for (unsigned i = 0; i < batch->ticks; ++i) {
            const auto input = pending.consume();
            const auto result = round.tick(input.horizontal,input.clockwise);
            if (result != study_round::Step::invalid && result != study_round::Step::stopped &&
                round.last_rotation_candidate() > 0)
                std::printf("rotation accepted: kick candidate=%d\n",round.last_rotation_candidate());
            if (result == study_round::Step::invalid) return 1;
            const bool locked = result == study_round::Step::locked ||
                                result == study_round::Step::game_over;
            if (locked) {
                cleared_this_frame += round.last_cleared();
                std::printf("score=%" PRIu64 " (+%" PRIu64 ") lines=%" PRIu64 " level=%u interval=%d ticks\n",
                    round.score(),round.last_awarded(),round.total_lines(),round.level(),round.gravity().interval);
            }
            board_changed = board_changed || locked;
            piece_changed = piece_changed || locked || result == study_round::Step::changed;
            if (result == study_round::Step::game_over)
                std::puts(study_ui::end_message(round.end_reason()));
        }
        if (cleared_this_frame > 0)
            std::printf("cleared %d row(s) this frame\n",cleared_this_frame);
        if (board_changed) {
            const auto next_mesh = study_board::make_mesh(round.board());
            if (!vertex_buffer.replace_same_size(next_mesh.vertices.data(),
                                                  next_mesh.vertices.size())) return 1;
            // Draw ranges must describe exactly the data that was just uploaded.
            mesh = next_mesh;
            const auto preview=study_next_view::make_mesh(round.next());
            if(!preview||preview->count!=72||
               !preview_buffer.replace_same_size(preview->vertices.data(),preview->count))return 1;
        }
        if (piece_changed && round.active()) {
            const auto updated_cells = study_piece::to_board(*round.active());
            if (!updated_cells) return 1;
            const auto updated_mesh = study_piece_view::make_visible_mesh(*updated_cells);
            if (!piece_buffer.replace_same_size(updated_mesh.vertices.data(),
                                                updated_mesh.count)) return 1;
        }
        // Recompute after the batch: board/pose can both change on a lock.
        if (piece_changed) {
            ghost = round.ghost();
            if (round.active() && !ghost) return 1;
            if (ghost) {
                const auto cells=study_piece::to_board(ghost->piece);
                if (!cells) return 1;
                const auto hint=study_piece_view::make_visible_mesh(*cells);
                if (!ghost_buffer.replace_same_size(hint.vertices.data(),hint.count)) return 1;
            }
        }
        if (round.score()!=displayed_score) {
            score_mesh=study_score_view::make_mesh(round.score());
            if (!score_buffer.replace_same_size(score_mesh.vertices.data(),score_mesh.vertices.size())) return 1;
            displayed_score=round.score();
        }
        const auto window=platform_window_size();
        const auto drawable=platform_drawable_size();
        const auto layout=study_letterbox::make_layout(
            {window.width,window.height},{drawable.width,drawable.height},
            study_board_scene::logical);
        if (layout) {
            if (window.width!=previous_window.width || window.height!=previous_window.height ||
                drawable.width!=previous_drawable.width || drawable.height!=previous_drawable.height) {
                const auto& v=layout->viewport;
                std::printf("window=%dx%d drawable=%dx%d viewport(top-left)=%d,%d %dx%d GL-y=%d\n",
                    window.width,window.height,drawable.width,drawable.height,
                    v.x,v.y,v.width,v.height,study_letterbox::gl_y(*layout));
                previous_window=window; previous_drawable=drawable;
            }
            const auto mouse=platform_window_mouse();
            const auto point=mouse.available ? study_letterbox::window_to_logical(
                *layout,{static_cast<double>(mouse.x),static_cast<double>(mouse.y)}) : std::nullopt;
            report_elapsed+=frame.dt;
            if (report_elapsed>=0.5) {
                const auto cell = point ? study_board::cell_at(point->x,point->y) : std::nullopt;
                if (cell) std::printf("board mouse: row=%d column=%d (pitch includes gap)\n",
                                      cell->row,cell->column);
                else std::puts("mouse: outside board or unavailable");
                report_elapsed=0;
            }
            if (!study_board_scene::render(gl,empty_program.name(),filled_program.name(),
                    vertex_array.name(),vertex_buffer.vertex_count(),mesh.empty_vertices,*layout)) return 1;
            if (ghost && !study_piece_view::render(gl,ghost_program.name(),ghost_array.name(),
                                                   ghost_buffer.vertex_count())) return 1;
            if (round.active() && !study_piece_view::render(gl,piece_program.name(),piece_array.name(),
                                          piece_buffer.vertex_count())) return 1;
            if(!study_next_view::render(gl,piece_program.name(),preview_array.name(),
                                       preview_buffer.vertex_count()))return 1;
            if(round.finished()&&!study_end_view::render(gl,end_program.name(),end_array.name(),end_buffer.vertex_count()))return 1;
            if (!study_score_view::render(gl,score_program.name(),score_array.name(),
                    score_buffer.vertex_count(),static_cast<study_gl::GLsizei>(score_mesh.count))) return 1;
            (void)platform_present();
        }
        // Invalid/minimized dimensions skip rendering, not frame pacing.
        platform_end_frame();
    }
    return 0;
}

static void usage(const char* executable) {
    std::printf("Usage: %s [I|J|L|O|S|T|Z] [normal|initial-blocked|next-blocked|clear-rescue|four-clears] (default T normal)\n", executable);
}

int main(int argc, char** argv) {
    if (argc == 2 && std::string_view(argv[1]) == "--help") {
        usage(argv[0]);
        return 0;
    }
    if (argc > 3) {
        usage(argv[0]);
        return 2;
    }

    const auto* selected = study_catalog::find_name(argc >= 2 ? argv[1] : "T");
    if (!selected) {
        usage(argv[0]);
        return 2;
    }
    const auto piece = study_catalog::make_piece(selected->kind);
    if (!piece) {
        return 1; // A known catalog entry must produce a Piece.
    }
    std::printf("selected=%.*s id=%d spawn=(%d,%d)\n",
        static_cast<int>(selected->name.size()), selected->name.data(),
        static_cast<int>(selected->kind), piece->origin.row, piece->origin.column);

    std::puts("Tap Left/Right: one cell. Tap Up: clockwise rotation (ordered kicks). Gravity: 30 ticks per cell. Right-side previews: next three kinds, top first.");
    auto scenario=spawn_example::Scenario::normal;
    bool four_clears=false;
    if(argc==3){
        const std::string_view name=argv[2];
        if(name=="initial-blocked")scenario=spawn_example::Scenario::initial_blocked;
        else if(name=="next-blocked")scenario=spawn_example::Scenario::next_blocked;
        else if(name=="clear-rescue")scenario=spawn_example::Scenario::clear_rescue;
        else if(name=="four-clears" && selected->kind==study_catalog::Kind::I)four_clears=true;
        else if(name!="normal"){usage(argv[0]);return 2;}
    }
    const auto board=four_clears?std::optional<study_grid::Grid>{make_four_clears()}:spawn_example::make(selected->kind,scenario);
    const auto source=four_clears?study_next::ScriptedSource::repeating(selected->kind):study_next::ScriptedSource::cycle(selected->kind);
    const auto round=source&&board?study_round::Round::create(*board,*source):std::nullopt;
    if(!round)return 1; // Invalid setup, distinct from a valid finished round.
    std::puts(four_clears ? "four-clears: repeating I source." : "Cycle follows catalog order.");
    if (!platform_init(640, 480, "Tetris study: score and level")) {
        return 1;
    }
    int result = 1;
    try {
        result = run_session(*round);
    } catch (const std::exception& e) {
        std::fprintf(stderr, "Session exception: %s\n", e.what());
    }
    platform_shutdown();
    return result;
}
