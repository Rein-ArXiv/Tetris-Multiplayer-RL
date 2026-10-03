#include "platform/platform.h"
#include "seed_option.h"
#include "client/application.h"
#include "renderer/flush_scene.h"
#include "renderer/texture_quad.h"
#include "renderer/image_decode.h"
#include "renderer/flush_device.h"
#include "renderer/batch_device.h"
#include "renderer/board_scene.h"
#include "renderer/gl_api.h"
#include "simulation/catalog.h"
#include "simulation/next_queue.h"
#include "simulation/round.h"
#include "simulation/state_hash.h"
#include "spawn_example.h"
#include "spin_example.h"
#include "score_example.h"
#include "end_message.h"
#include <cinttypes>
#include <cstdio>
#include <exception>
#include <optional>
#include <string_view>

// The table and borrowed GL strings are used only while the context exists.
static int run_session(study_round::Round initial_round, const char* image_path)
{
    study_app::Application app(initial_round);
    std::puts("Menu: Space starts; Escape quits. Play: Space drops; Escape returns to menu.");
    std::puts("Game over: Space restarts the same setup. Release gameplay keys after a transition.");
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

    // Device owns one VBO/VAO/program; destroyed before the GL context.
    study_batch::Device device(gl);
    if (!device.init() || !study_board_scene::configure(gl)) return 1;
    study_texture::Texture badge(gl);
    {
        const auto decoded=study_image::decode_file(std::filesystem::u8path(image_path));
        if(!decoded){std::fprintf(stderr,"Badge: %s\n",study_image::error_text(decoded.error));return 1;}
        if(!badge.upload(decoded.image->view()))return 1;
    } // CPU pixels are no longer needed after the upload returns.
    study_texture::Quad badge_quad(gl);
    if (!badge_quad.init()) return 1;
    std::uint64_t last_vertices=0, last_draws=0;
    while (!platform_should_close()) {
        const FrameInfo frame=platform_begin_frame();
        if (platform_should_close()) break;
        const study_loop::FrameInput raw{
            platform_key_pressed(Key::Left),platform_key_pressed(Key::Right),
            platform_key_pressed(Key::Up),platform_key_down(Key::Down),
            platform_key_pressed(Key::Space),platform_input_cancelled()};
        const bool released=!platform_key_down(Key::Left)&&!platform_key_down(Key::Right)&&
            !platform_key_down(Key::Up)&&!platform_key_down(Key::Down)&&!platform_key_down(Key::Space);
        const auto update=app.advance(frame.dt,raw,platform_key_pressed(Key::Space),
                                      platform_key_pressed(Key::Escape),released);
        if (!update) return 1;
        if (app.screen()==study_app::Screen::quitting) break;
        study_blend::Rgba background{.04,.07,.12,1};
        std::optional<study_game::GameView> view;
        if (app.screen()!=study_app::Screen::menu) {
            const auto& game=*app.game();
            const auto& round=game.round();
            view=game.view();
            if (!view) return 1;
            background=game.background();
            const auto report=update->frame.value_or(study_loop::FrameReport{});
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

        }
        const auto window=platform_window_size();
        const auto drawable=platform_drawable_size();
        const auto layout=study_letterbox::make_layout(
            {window.width,window.height},{drawable.width,drawable.height},study_board_scene::logical);
        if (layout) {
            if (!study_letterbox_scene::begin(gl,*layout,background)) return 1;
            const auto full=study_flush::full_clip(*layout);
            const auto board_clip=study_flush::project_clip(*layout,{110,20,100,200});
            if (!board_clip) return 1;
            study_flush::GlSink sink{gl,device};
            study_flush::Stream<study_flush::GlSink> stream(sink,full);
            const bool built=view ? study_flush::scene(stream,*view,full,*board_clip)
                                  : study_flush::menu(stream);
            if (!built || !stream.finish()) return 1;
            const auto stats=stream.statistics();
            if (last_vertices!=stats.vertices || last_draws!=stats.draws) {
                std::printf("opaque stream: %llu vertices, %llu ordered draw submissions\n",
                    static_cast<unsigned long long>(stats.vertices),
                    static_cast<unsigned long long>(stats.draws));
                last_vertices=stats.vertices;last_draws=stats.draws;
            }
            // Stream is finished: changing the program/texture is now ordered.
            if (!sink.set_clip(full) || !badge_quad.draw(badge)) return 1;
            (void)platform_present();
        }
        platform_end_frame();
    }
    return 0;
}

static void usage(const char* executable) {
    std::puts("Optional final arguments: --image PATH (default assets/player.png relative to working directory)");
    std::printf("Seeded bag: %s --seed DECIMAL [garbage] (0..18446744073709551615)\n", executable);
    std::printf("Usage: %s [I|J|L|O|S|T|Z] [normal|spin|garbage|initial-blocked|next-blocked|clear-rescue|four-clears] (default T normal)\n", executable);
}

int main(int argc, char** argv) {
    const char* image_path="assets/player.png";
    if(argc>=3 && std::string_view(argv[argc-2])=="--image") {
        image_path=argv[argc-1];argc-=2;
    }
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
        result = run_session(*round,image_path);
    } catch (const std::exception& e) {
        std::fprintf(stderr, "Session exception: %s\n", e.what());
    }
    platform_shutdown();
    return result;
}
