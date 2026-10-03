#include "platform/platform.h"
#include "board_example.h"
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

// The table and borrowed GL strings are used only while the context exists.
static int run_session(const study_grid::Grid& board)
{
    std::size_t occupied=0;
    for(const auto cell:board.cells()) if(cell==study_grid::Cell::filled) ++occupied;
    std::printf("board state: %d rows x %d columns, %zu occupied cells\n",
                study_grid::Grid::kRows,study_grid::Grid::kColumns,occupied);
    const auto mesh = study_board::make_mesh(board);
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

    WindowSize previous_window{};
    DrawableSize previous_drawable{};
    double report_elapsed=0;
    while (!platform_should_close()) {
        const FrameInfo frame=platform_begin_frame();
        if (platform_should_close() || platform_key_pressed(Key::Escape)) break;
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
            (void)platform_present();
        }
        // Invalid/minimized dimensions skip rendering, not frame pacing.
        platform_end_frame();
    }
    return 0;
}

int main()
{
    const auto board=make_example_board();
    if (!platform_init(640,480,"Tetris study: board rendering")) return 1;
    int result=1;
    try { result=run_session(board); }
    catch (const std::exception& e) { std::fprintf(stderr,"Session exception: %s\n",e.what()); }
    platform_shutdown();
    return result;
}
