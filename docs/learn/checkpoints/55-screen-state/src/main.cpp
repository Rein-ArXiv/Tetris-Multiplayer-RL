#include "platform/platform.h"
#include "seed_option.h"
#include "client/application.h"
#include "simulation/state_hash.h"
#include "spawn_example.h"
#include "spin_example.h"
#include "score_example.h"
#include "end_message.h"
#include "renderer/end_marker.h"
#include "renderer/score_view.h"
#include "renderer/ghost_view.h"
#include <cinttypes>
#include "simulation/round.h"
#include "loop/frame_runner.h"
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
static int run_session(study_round::Round initial_round)
{
    study_app::Application app(initial_round);
    // Prepare GPU capacity from the baseline; the menu owns no live game.
    auto view = study_game::make_view(initial_round);
    if (!view) return 1;
    std::puts("Menu: Space starts; Escape quits. Play: Space drops; Escape returns to menu.");
    std::puts("Game over: Space restarts the same setup. Release gameplay keys after a transition.");
    std::puts("Hold Down: immediate first sampled tick, repeat every 4 ticks; release resets.");
    const auto& board = view->board;
    std::size_t occupied=0;
    for(const auto cell:board.cells()) if(cell==study_grid::Cell::filled) ++occupied;
    std::printf("board state: %d rows x %d columns, %zu occupied cells\n",
                study_grid::Grid::kRows,study_grid::Grid::kColumns,occupied);
    auto mesh = study_board::make_mesh(board);
    study_piece_view::Mesh piece_mesh;
    if(view->active){
        const auto cells=study_piece::to_board(*view->active);
        if(!cells)return 1;
        piece_mesh=study_piece_view::make_visible_mesh(*cells);
    }
    if((view->end_reason != study_round::EndReason::none))std::puts(study_ui::end_message(view->end_reason));
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
    auto ghost = view->ghost;
    if (view->active && !ghost) return 1;
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

    auto displayed_score = view->score;
    auto score_mesh = study_score_view::make_mesh(displayed_score);
    study_gl::VertexBuffer score_buffer(gl);study_gl::VertexArray score_array(gl);
    study_gl::Program score_program(gl);
    if (!score_buffer.upload(score_mesh.vertices) ||
        !score_array.configure(score_buffer.name()) ||
        !study_board_scene::link_program(gl,score_program,study_score_view::fragment)) return 1;
    const auto preview=study_next_view::make_mesh(view->next);
    if(!preview||preview->count!=72)return 1;
    study_gl::VertexBuffer preview_buffer(gl);
    study_gl::VertexArray preview_array(gl);
    if(!preview_buffer.upload(preview->vertices.data(),preview->count)||
       !preview_array.configure(preview_buffer.name()))return 1;
    // A play triangle uses the same opaque geometry path as the board.
    const study_mesh::Triangle menu_mesh{{{-0.25f,-0.4f},{0.35f,0.f},{-0.25f,0.4f}}};
    study_gl::VertexBuffer menu_buffer(gl);
    study_gl::VertexArray menu_array(gl);
    if (!menu_buffer.upload(menu_mesh) || !menu_array.configure(menu_buffer.name())) return 1;
    WindowSize previous_window{};
    DrawableSize previous_drawable{};
    double report_elapsed=0;
    while (!platform_should_close()) {
        const FrameInfo frame=platform_begin_frame();
        if (platform_should_close()) break;
        const study_loop::FrameInput raw{
            platform_key_pressed(Key::Left),platform_key_pressed(Key::Right),
            platform_key_pressed(Key::Up),platform_key_down(Key::Down),
            platform_key_pressed(Key::Space),
                                           platform_input_cancelled()};
        const bool released = !platform_key_down(Key::Left) && !platform_key_down(Key::Right) &&
            !platform_key_down(Key::Up) && !platform_key_down(Key::Down) && !platform_key_down(Key::Space);
        const auto update = app.advance(frame.dt, raw, platform_key_pressed(Key::Space),
                                        platform_key_pressed(Key::Escape), released);
        if (!update) return 1;
        if (app.screen() == study_app::Screen::quitting) break;
        if (app.screen() == study_app::Screen::menu) {
            const auto window = platform_window_size();
            const auto drawable = platform_drawable_size();
            const auto layout = study_letterbox::make_layout(
                {window.width,window.height}, {drawable.width,drawable.height}, study_board_scene::logical);
            if (layout) {
                if (!study_letterbox_scene::begin(gl,*layout,{0.04,0.07,0.12,1}) ||
                    !study_gl::submit_triangles(gl,filled_program.name(),menu_array.name(),3,0,3)) return 1;
                (void)platform_present();
            }
            platform_end_frame();
            continue;
        }
        // Borrow only after transitions. Never carry this reference across advance().
        const auto& game = *app.game();
        const auto& round = game.round();
        auto report = update->frame.value_or(study_loop::FrameReport{});
        if (update->game_replaced) {
            report.board_changed = report.piece_changed = true;
            report_elapsed = 0;
        }
        if (report.board_changed || report.piece_changed || round.score() != view->score) {
            view = game.view();
            if (!view) return 1;
        }
        // One observation after all simulation ticks scheduled in this frame.
        if (report.ticks > 0) {
            const auto digest = study_hash::state_hash(round);
            if (!digest) return 1;
            std::printf("round LRND/1 after frame batch (%u ticks): %016" PRIx64 "\n",
                        report.ticks, *digest);
        }
        // Reports own per-tick values; the Round reference now shows final state.
        for (unsigned i=0;i<report.ticks;++i) {
            const auto& observation=report.observations[i];
            if (observation.kick>0)
                std::printf("rotation accepted: kick candidate=%d\n",observation.kick);
            if (observation.lock) {
                const auto& lock=*observation.lock;
                std::printf("attack total=%llu pending=%d inserted=%d\n",
                    static_cast<unsigned long long>(lock.attack_total),lock.pending,lock.inserted);
                std::printf("spin lines=%d clear streak=%llu\n",lock.spin_lines,
                    static_cast<unsigned long long>(lock.streak));
                if (lock.hard_drop_distance>=0)
                    std::printf("hard drop: %d row(s) before lock\n",lock.hard_drop_distance);
                std::printf("score=%" PRIu64 " (+%" PRIu64 ") lines=%" PRIu64 " level=%u interval=%d ticks\n",
                    lock.points,lock.awarded,lock.lines,lock.level,lock.interval);
                if(lock.end_reason!=study_round::EndReason::none)
                    std::puts(study_ui::end_message(lock.end_reason));
            }
        }
        if (report.cleared>0)std::printf("cleared %d row(s) this frame\n",report.cleared);
        const bool board_changed=report.board_changed,piece_changed=report.piece_changed;
        if (board_changed) {
            const auto next_mesh = study_board::make_mesh(view->board);
            if (!vertex_buffer.replace_same_size(next_mesh.vertices.data(),
                                                  next_mesh.vertices.size())) return 1;
            // Draw ranges must describe exactly the data that was just uploaded.
            mesh = next_mesh;
            const auto preview=study_next_view::make_mesh(view->next);
            if(!preview||preview->count!=72||
               !preview_buffer.replace_same_size(preview->vertices.data(),preview->count))return 1;
        }
        if (piece_changed && view->active) {
            const auto updated_cells = study_piece::to_board(*view->active);
            if (!updated_cells) return 1;
            const auto updated_mesh = study_piece_view::make_visible_mesh(*updated_cells);
            if (piece_buffer.name()) {
                if (!piece_buffer.replace_same_size(updated_mesh.vertices.data(),updated_mesh.count)) return 1;
            } else if (!piece_buffer.upload(updated_mesh.vertices.data(),updated_mesh.count) ||
                       !piece_array.configure(piece_buffer.name())) return 1;
        }
        // Recompute after the batch: board/pose can both change on a lock.
        if (piece_changed) {
            ghost = view->ghost;
            if (view->active && !ghost) return 1;
            if (ghost) {
                const auto cells=study_piece::to_board(ghost->piece);
                if (!cells) return 1;
                const auto hint=study_piece_view::make_visible_mesh(*cells);
                if (ghost_buffer.name()) {
                    if (!ghost_buffer.replace_same_size(hint.vertices.data(),hint.count)) return 1;
                } else if (!ghost_buffer.upload(hint.vertices.data(),hint.count) ||
                           !ghost_array.configure(ghost_buffer.name())) return 1;
            }
        }
        if (view->score!=displayed_score) {
            score_mesh=study_score_view::make_mesh(view->score);
            if (!score_buffer.replace_same_size(score_mesh.vertices.data(),score_mesh.vertices.size())) return 1;
            displayed_score=view->score;
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
                    vertex_array.name(),vertex_buffer.vertex_count(),mesh.empty_vertices,*layout,game.background())) return 1;
            if (ghost && !study_piece_view::render(gl,ghost_program.name(),ghost_array.name(),
                                                   ghost_buffer.vertex_count())) return 1;
            if (view->active && !study_piece_view::render(gl,piece_program.name(),piece_array.name(),
                                          piece_buffer.vertex_count())) return 1;
            if(!study_next_view::render(gl,piece_program.name(),preview_array.name(),
                                       preview_buffer.vertex_count()))return 1;
            if((view->end_reason != study_round::EndReason::none)&&!study_end_view::render(gl,end_program.name(),end_array.name(),end_buffer.vertex_count()))return 1;
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
    std::printf("Seeded bag: %s --seed DECIMAL [garbage] (0..18446744073709551615)\n", executable);
    std::printf("Usage: %s [I|J|L|O|S|T|Z] [normal|spin|garbage|initial-blocked|next-blocked|clear-rescue|four-clears] (default T normal)\n", executable);
}

int main(int argc, char** argv) {
    if (argc == 2 && std::string_view(argv[1]) == "--help") {
        usage(argv[0]);
        return 0;
    }
    if (argc > 4 || (argc == 4 && (std::string_view(argv[1]) != "--seed" || std::string_view(argv[3]) != "garbage"))) {
        usage(argv[0]);
        return 2;
    }

    std::optional<study_round::Round> round;
    if ((argc == 3 || argc == 4) && std::string_view(argv[1]) == "--seed") {
        const auto seed = study_seed::parse(argv[2]);
        if (!seed) { usage(argv[0]); return 2; }
        round = study_round::Round::create_seeded(study_grid::Grid{}, *seed);
        if (!round) return 1;
        if (argc == 4) {
            if (!round->add_garbage(3)) return 1;
            std::puts("3 pending rows: one seeded garbage hole on the next lock.");
        }
        std::printf("Seeded 7-bag: requested=%" PRIu64 ", effective=%" PRIu64 "\n",
                    *seed, *seed ? *seed : study_next::SeededBagSource::default_seed);
        std::printf("current=%.*s preview=", 1, study_catalog::find(round->kind())->name.data());
        for (std::size_t i = 0; i < study_next::Queue::capacity; ++i)
            std::printf("%.*s", 1, study_catalog::find(*round->next().peek(i))->name.data());
        std::puts("");
    } else {
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

        auto scenario=spawn_example::Scenario::normal;
        bool four_clears=false, garbage=false, spin=false;
        if(argc==3){
            const std::string_view name=argv[2];
            if(name=="spin" && selected->kind==study_catalog::Kind::T)spin=true;
            else if(name=="garbage")garbage=true;
            else if(name=="initial-blocked")scenario=spawn_example::Scenario::initial_blocked;
            else if(name=="next-blocked")scenario=spawn_example::Scenario::next_blocked;
            else if(name=="clear-rescue")scenario=spawn_example::Scenario::clear_rescue;
            else if(name=="four-clears" && selected->kind==study_catalog::Kind::I)four_clears=true;
            else if(name!="normal"){usage(argv[0]);return 2;}
        }
        const auto board=spin?std::optional<study_grid::Grid>{make_spin_board()}:four_clears?std::optional<study_grid::Grid>{make_four_clears()}:spawn_example::make(selected->kind,scenario);
        const auto source=four_clears?study_next::ScriptedSource::repeating(selected->kind):study_next::ScriptedSource::cycle(selected->kind);
        round=source&&board?study_round::Round::create(*board,*source):std::nullopt;
        if(!round)return 1; // Invalid setup, distinct from a valid finished round.
        if(garbage && !round->add_garbage(3))return 1;
        if(spin)std::puts("T-spin exercise: rotate once with Up, then Space before gravity moves it. See lock report.");
        if(garbage)std::puts("3 pending garbage rows: inserted after your next lock; scripted hole starts at column4.");
        std::puts(four_clears ? "four-clears: repeating I source." : "Cycle follows catalog order.");
    }
    std::puts("Tap Left/Right: one cell. Tap Up: clockwise rotation (ordered kicks). Gravity: 30 ticks per cell. Right-side previews: next three kinds, top first.");
    if (!platform_init(640, 480, "Tetris study: Space start/drop/restart - Escape back/quit")) {
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
