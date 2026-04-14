cmake_minimum_required(VERSION 3.15)
project(tetris CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# MSVC: UTF-8 소스 파일 인코딩 (한국어 주석 깨짐 방지)
if (MSVC)
    add_compile_options(/utf-8)
endif()

# -----------------------------------------------------------------------------
# Build options
#
#   TETRIS_BUILD_GAME — build the Handmade Win32+OpenGL game executable.
#                        Requires a Win32 environment (platform/win32.cpp).
#                        No raylib dependency.
#   TETRIS_BUILD_PY   — build the pybind11 module (tetris_py) that wraps SimGame
#                        for Python. Raylib-free. This is what Colab needs for training.
#   TETRIS_BUILD_TEST — build the determinism regression test (sim_hash_dump)
#                        which exercises SimGame only (no rendering).
# -----------------------------------------------------------------------------
option(TETRIS_BUILD_GAME "Build the handmade Win32+OpenGL game executable" ON)
option(TETRIS_BUILD_PY   "Build the pybind11 module (tetris_py)" OFF)
option(TETRIS_BUILD_TEST "Build the SimGame determinism test" ON)

# -----------------------------------------------------------------------------
# Sources shared between all targets
# -----------------------------------------------------------------------------
# Pure (no raylib) logic — used by game, pybind11 module, and tests.
set(TETRIS_SIM_SOURCES
    src/sim_game.cpp
    src/position.cpp
)

set(TETRIS_SIM_HEADERS
    src/sim_game.h
    src/sim_grid.h
    src/sim_block.h
    src/sim_blocks.h
    src/position.h
    core/constants.h
    core/input.h
    core/rng.h
    core/hash.h
)

# -----------------------------------------------------------------------------
# Target: tetris (Handmade Win32 + OpenGL game)
# -----------------------------------------------------------------------------
if (TETRIS_BUILD_GAME)
    # Handmade renderer: Win32 window + OpenGL context, no raylib.
    # platform/win32.cpp  — window, input, timer (Win32 API)
    # renderer/renderer.cpp — draw_rect / draw_text (OpenGL 2.0+)
    set(TETRIS_GAME_SOURCES
        ${TETRIS_SIM_SOURCES}
        src/main.cpp
        src/game.cpp
        src/colors.cpp
        core/replay.cpp
        net/socket.cpp
        net/framing.cpp
        net/session.cpp
        platform/win32.cpp
        renderer/renderer.cpp
        audio/audio.cpp
    )

    add_executable(tetris
        ${TETRIS_GAME_SOURCES}
        ${TETRIS_SIM_HEADERS}
        src/game.h
        src/colors.h
        core/replay.h
        net/socket.h
        net/framing.h
        net/session.h
        platform/platform.h
        renderer/renderer.h
        renderer/shaders.h
        audio/audio.h
    )

    target_include_directories(tetris PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

    if (WIN32)
        # No raylib. Link only what Win32 + OpenGL needs.
        target_link_libraries(tetris PRIVATE opengl32 gdi32 winmm ws2_32 xaudio2 ole32)
    else()
        message(FATAL_ERROR "Handmade renderer is Win32-only. Use Linux/Mac only for pybind11/test targets.")
    endif()

    # Copy assets (fonts + sounds)
    add_custom_target(copy_assets ALL
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/Font ${CMAKE_CURRENT_BINARY_DIR}/Font
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/Sounds ${CMAKE_CURRENT_BINARY_DIR}/Sounds
        DEPENDS tetris
    )
endif()

# -----------------------------------------------------------------------------
# Target: tetris_py (pybind11 module — Colab + local Python bot)
# -----------------------------------------------------------------------------
if (TETRIS_BUILD_PY)
    # cmake 4.0+ removed FindPythonInterp/FindPythonLibs; tell pybind11 to use
    # the modern FindPython instead.
    set(PYBIND11_FINDPYTHON ON)
    # pybind11: prefer find_package (pip-installed), fall back to add_subdirectory
    # if a vendored pybind11 checkout is provided.
    find_package(pybind11 CONFIG QUIET)
    if (NOT pybind11_FOUND)
        message(FATAL_ERROR
            "pybind11 not found. Install it (pip install pybind11) and "
            "re-run cmake with -Dpybind11_DIR=$(python -m pybind11 --cmakedir)")
    endif()

    pybind11_add_module(tetris_py
        bindings/tetris_py.cpp
        ${TETRIS_SIM_SOURCES}
        ${TETRIS_SIM_HEADERS}
    )

    target_include_directories(tetris_py PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
endif()

# -----------------------------------------------------------------------------
# Target: sim_hash_dump (determinism regression test — raylib-free)
# -----------------------------------------------------------------------------
if (TETRIS_BUILD_TEST)
    add_executable(sim_hash_dump
        tests/sim_hash_dump.cpp
        ${TETRIS_SIM_SOURCES}
        ${TETRIS_SIM_HEADERS}
    )
    target_include_directories(sim_hash_dump PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
endif()
