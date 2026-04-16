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
#   TETRIS_BUILD_GAME  — Handmade Win32+OpenGL game executable.
#                         Currently Win32-only. SDL2 크로스플랫폼 경로는 Part A 에서.
#   TETRIS_BUILD_PY    — pybind11 module (tetris_py) wrapping SimGame. Portable.
#   TETRIS_BUILD_TEST  — SimGame determinism regression (sim_hash_dump). Portable.
#   TETRIS_BUILD_RELAY — Headless matchmaking/relay server (server/*.cpp).
#                         No GUI/audio. Linux(Termux), macOS, Windows 모두 OK.
# -----------------------------------------------------------------------------
option(TETRIS_BUILD_GAME  "Build the handmade game executable"              ON)
option(TETRIS_BUILD_PY    "Build the pybind11 module (tetris_py)"           OFF)
option(TETRIS_BUILD_TEST  "Build the SimGame determinism test"              ON)
option(TETRIS_BUILD_RELAY "Build the tetris_relay matchmaking server"       OFF)

# TETRIS_USE_SDL2 — Use the SDL2 cross-platform backend (window + audio + text).
# Default ON on non-Windows so macOS/Linux users get it automatically.
# On Windows, default OFF to preserve the handmade Win32+OpenGL path.
if (WIN32)
    option(TETRIS_USE_SDL2 "Use SDL2 backend (cross-platform)" OFF)
else()
    option(TETRIS_USE_SDL2 "Use SDL2 backend (cross-platform)" ON)
endif()

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
    # 공통: 시뮬레이션 + 게임 로직 + 렌더러 공통 부분 + 네트워킹
    set(TETRIS_GAME_COMMON
        ${TETRIS_SIM_SOURCES}
        src/main.cpp
        src/game.cpp
        src/colors.cpp
        core/replay.cpp
        net/socket.cpp
        net/framing.cpp
        net/session.cpp
        renderer/renderer.cpp
    )

    set(TETRIS_GAME_HEADERS
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

    if (TETRIS_USE_SDL2)
        # SDL2 경로: Mac/Linux (+ 옵션으로 Windows). 통합 백엔드 하나.
        find_package(SDL2 REQUIRED)

        add_executable(tetris
            ${TETRIS_GAME_COMMON}
            ${TETRIS_GAME_HEADERS}
            platform/sdl.cpp
            renderer/text_stb.cpp
            audio/sdl_audio.cpp
        )
        target_include_directories(tetris PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${SDL2_INCLUDE_DIRS})

        # SDL2::SDL2 타겟은 find_package(SDL2) 배포 버전마다 제공 여부가 다름
        if (TARGET SDL2::SDL2)
            target_link_libraries(tetris PRIVATE SDL2::SDL2)
        else()
            target_link_libraries(tetris PRIVATE ${SDL2_LIBRARIES})
        endif()

        if (APPLE)
            target_link_libraries(tetris PRIVATE "-framework OpenGL")
        elseif (WIN32)
            target_link_libraries(tetris PRIVATE opengl32 ws2_32)
        else()
            find_package(OpenGL REQUIRED)
            target_link_libraries(tetris PRIVATE OpenGL::GL)
            find_package(Threads REQUIRED)
            target_link_libraries(tetris PRIVATE Threads::Threads)
        endif()
    else()
        # Handmade 경로: Win32 window + OpenGL + XAudio2 + GDI text
        add_executable(tetris
            ${TETRIS_GAME_COMMON}
            ${TETRIS_GAME_HEADERS}
            platform/win32.cpp
            renderer/text_win32.cpp
            audio/audio.cpp
        )
        target_include_directories(tetris PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

        if (WIN32)
            target_link_libraries(tetris PRIVATE opengl32 gdi32 winmm ws2_32 xaudio2 ole32)
        else()
            message(FATAL_ERROR "Handmade Win32 backend is Windows-only. Set -DTETRIS_USE_SDL2=ON.")
        endif()
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

# -----------------------------------------------------------------------------
# Target: tetris_relay (matchmaking / relay server)
#
# 역할: TCP accept → QUEUE_JOIN 수신 → 2명 모이면 MATCH_FOUND 전송 + 바이트 포워딩.
#       GUI/오디오/시뮬 없음 — net/ 만 재사용. Termux(Ubuntu proot ARM64) 포함 크로스.
# -----------------------------------------------------------------------------
if (TETRIS_BUILD_RELAY)
    add_executable(tetris_relay
        server/main.cpp
        server/matchmaker.cpp
        server/player_conn.cpp
        server/relay.cpp
        net/socket.cpp
        net/framing.cpp
        server/matchmaker.h
        server/player_conn.h
        server/relay.h
        net/socket.h
        net/framing.h
    )
    target_include_directories(tetris_relay PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    if (WIN32)
        target_link_libraries(tetris_relay PRIVATE ws2_32)
    else()
        # Linux/macOS: std::thread 는 pthread 를 필요로 함 (libstdc++)
        find_package(Threads REQUIRED)
        target_link_libraries(tetris_relay PRIVATE Threads::Threads)
    endif()
endif()
