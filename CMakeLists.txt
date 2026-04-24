cmake_minimum_required(VERSION 3.15)
# C 언어도 활성화 — third_party/sqlite3.c (amalgamation) 를 빌드하려면 필요.
# tetris_meta 타겟만 C 를 쓰지만 enable_language 는 프로젝트 루트에서 선언해야 한다.
project(tetris CXX C)

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
# TETRIS_BUILD_META — HTTP + SQLite metadata server (guest/auth/matches/leaderboard).
# Typically deployed on a separate machine (e.g. Mac mini) to keep the relay stateless.
option(TETRIS_BUILD_META  "Build the tetris_meta HTTP+SQLite metadata server" OFF)
# TETRIS_BUILD_BOT — Section C: link onnxruntime and compile bot/*.cpp.
# OFF 이면 bot_onnx 가 "not vendored" 스텁으로 빌드되어 실행 시 모델 로드가
# 항상 실패 → 메뉴에서 Single vs Bot 이 회색 비활성으로 표시된다.
option(TETRIS_BUILD_BOT   "Link onnxruntime (Section C bot inference)"      OFF)

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
    # 공통: 시뮬레이션 + 게임 로직 + 렌더러 공통 부분 + 네트워킹 + 봇
    #   bot/*.cpp 는 TETRIS_BUILD_BOT 과 관계없이 항상 컴파일한다 — OFF 일 때
    #   bot_onnx.cpp 는 자동으로 스텁 모드(TETRIS_HAS_ONNXRUNTIME 미정의)로
    #   빌드되어 main.cpp 의 호출부만 정상 링크된다.
    # meta/http_client.cpp 는 tetris_meta 서버와의 HTTP 통신 (guest 토큰 발급용).
    # third_party/httplib.h 가 있어야 한다 — 없으면 빌드 실패로 빠르게 감지.
    if (NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/httplib.h")
        message(FATAL_ERROR
            "TETRIS_BUILD_GAME=ON 이지만 third_party/httplib.h 가 없습니다. "
            "tetris_meta 서버 호출 (guest 토큰) 용. 다운로드 후 재시도.")
    endif()
    set(TETRIS_GAME_COMMON
        ${TETRIS_SIM_SOURCES}
        src/main.cpp
        src/game.cpp
        src/gui.cpp
        src/colors.cpp
        core/replay.cpp
        net/socket.cpp
        net/framing.cpp
        net/session.cpp
        renderer/renderer.cpp
        renderer/shake.cpp
        renderer/image.cpp
        bot/placement.cpp
        bot/bot_onnx.cpp
        meta/http_client.cpp
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
        renderer/shake.h
        renderer/image.h
        audio/audio.h
        bot/placement.h
        bot/bot_onnx.h
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
        target_include_directories(tetris PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}
            ${CMAKE_CURRENT_SOURCE_DIR}/third_party
            ${SDL2_INCLUDE_DIRS})

        # SDL2::SDL2 타겟은 find_package(SDL2) 배포 버전마다 제공 여부가 다름
        if (TARGET SDL2::SDL2)
            target_link_libraries(tetris PRIVATE SDL2::SDL2)
        else()
            target_link_libraries(tetris PRIVATE ${SDL2_LIBRARIES})
        endif()

        if (APPLE)
            target_link_libraries(tetris PRIVATE "-framework OpenGL")
        elseif (WIN32)
            target_link_libraries(tetris PRIVATE opengl32 gdiplus ws2_32)
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
        target_include_directories(tetris PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}
            ${CMAKE_CURRENT_SOURCE_DIR}/third_party)

        if (WIN32)
            target_link_libraries(tetris PRIVATE opengl32 gdi32 gdiplus winmm ws2_32 xaudio2 ole32)
        else()
            message(FATAL_ERROR "Handmade Win32 backend is Windows-only. Set -DTETRIS_USE_SDL2=ON.")
        endif()
    endif()

    # ------------------------------------------------------------------------
    # Optional: ONNX Runtime for Section C (Single vs Bot inference)
    # third_party/onnxruntime/ 에 공식 CPU 번들을 풀어두면 링크된다.
    # 없거나 OFF 면 bot/bot_onnx.cpp 가 스텁으로 빌드됨 → Load 항상 실패.
    # ------------------------------------------------------------------------
    if (TETRIS_BUILD_BOT)
        set(ORT_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/third_party/onnxruntime")
        if (NOT EXISTS "${ORT_ROOT}/include/onnxruntime_cxx_api.h")
            message(FATAL_ERROR
                "TETRIS_BUILD_BOT=ON 이지만 ${ORT_ROOT}/include/onnxruntime_cxx_api.h 가 없습니다. "
                "third_party/fetch_onnxruntime.sh 로 벤더링하거나 TETRIS_BUILD_BOT=OFF 로 빌드하세요.")
        endif()
        target_compile_definitions(tetris PRIVATE TETRIS_HAS_ONNXRUNTIME=1)
        target_include_directories(tetris PRIVATE "${ORT_ROOT}/include")
        if (WIN32)
            target_link_libraries(tetris PRIVATE "${ORT_ROOT}/lib/win-x64/onnxruntime.lib")
        elseif (APPLE)
            target_link_libraries(tetris PRIVATE "${ORT_ROOT}/lib/osx-universal2/libonnxruntime.dylib")
        else()
            target_link_libraries(tetris PRIVATE "${ORT_ROOT}/lib/linux-x64/libonnxruntime.so")
        endif()
    endif()

    # ------------------------------------------------------------------------
    # Section G — 플랫폼별 배포 설정 (rpath, macOS .app 번들 메타)
    # ------------------------------------------------------------------------
    if (APPLE)
        # macOS .app 번들 — scripts/release_macos.sh 가 이 구조를 전제.
        # Info.plist 를 configure_file 로 @변수@ 치환 후 빌드 디렉터리에 생성.
        set(MACOSX_BUNDLE_GUI_IDENTIFIER "com.rein.tetris")
        set(MACOSX_BUNDLE_BUNDLE_NAME "Tetris")
        if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/platform/macos/Info.plist.in")
            if (NOT DEFINED PROJECT_VERSION)
                set(PROJECT_VERSION "1.0.0")
            endif()
            configure_file(
                "${CMAKE_CURRENT_SOURCE_DIR}/platform/macos/Info.plist.in"
                "${CMAKE_CURRENT_BINARY_DIR}/Info.plist"
                @ONLY)
        endif()
        # dylib 탐색: @executable_path/../Frameworks 를 rpath 에 추가.
        set_target_properties(tetris PROPERTIES
            BUILD_RPATH "@executable_path/../Frameworks"
            INSTALL_RPATH "@executable_path/../Frameworks")
    elseif (UNIX)
        # Linux — 번들에 동봉된 .so 를 찾기 위해 $ORIGIN/lib 을 rpath 로 설정.
        set_target_properties(tetris PROPERTIES
            BUILD_RPATH "$ORIGIN/lib"
            INSTALL_RPATH "$ORIGIN/lib")
    endif()

    # tetris_relay 도 Linux rpath 필요 (릴레이 단독 배포 시)
    if (UNIX AND NOT APPLE AND TETRIS_BUILD_RELAY)
        # tetris_relay 는 아래에서 별도 정의되지만, 이 시점에 아직 없을 수 있으므로
        # TETRIS_BUILD_RELAY 블록에서 직접 설정한다 (아래 참조).
    endif()

    # Copy assets (fonts + sounds + icons + model)
    #   assets/ 와 model/ 은 없을 수도 있으므로 directory 존재 검사 후 복사.
    set(_copy_cmds
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/Font   ${CMAKE_CURRENT_BINARY_DIR}/Font
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/Sounds ${CMAKE_CURRENT_BINARY_DIR}/Sounds
    )
    if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/assets")
        list(APPEND _copy_cmds
            COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/assets ${CMAKE_CURRENT_BINARY_DIR}/assets)
    endif()
    if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/model")
        list(APPEND _copy_cmds
            COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/model ${CMAKE_CURRENT_BINARY_DIR}/model)
    endif()
    add_custom_target(copy_assets ALL
        ${_copy_cmds}
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
    # relay 가 meta HTTP API 를 호출하려면 httplib 헤더와 http_client.cpp 필요.
    # third_party/httplib.h 는 TETRIS_BUILD_META 와 공유 — 릴레이만 빌드해도 필요.
    if (NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/httplib.h")
        message(FATAL_ERROR
            "TETRIS_BUILD_RELAY=ON 이지만 third_party/httplib.h 가 없습니다. "
            "meta API 호출용. cpp-httplib 를 다운로드해 third_party/ 에 넣으세요.")
    endif()
    add_executable(tetris_relay
        server/main.cpp
        server/matchmaker.cpp
        server/player_conn.cpp
        server/relay.cpp
        server/room.cpp
        net/socket.cpp
        net/framing.cpp
        meta/http_client.cpp
        server/matchmaker.h
        server/player_conn.h
        server/relay.h
        server/room.h
        net/socket.h
        net/framing.h
        meta/http_client.h
        meta/protocol.h
    )
    target_include_directories(tetris_relay PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party
    )
    if (WIN32)
        target_link_libraries(tetris_relay PRIVATE ws2_32)
    else()
        # Linux/macOS: std::thread 는 pthread 를 필요로 함 (libstdc++)
        find_package(Threads REQUIRED)
        target_link_libraries(tetris_relay PRIVATE Threads::Threads)
    endif()
    # Section G: Linux 배포 번들용 rpath
    if (UNIX AND NOT APPLE)
        set_target_properties(tetris_relay PROPERTIES
            BUILD_RPATH "$ORIGIN/lib"
            INSTALL_RPATH "$ORIGIN/lib")
    endif()
endif()

# -----------------------------------------------------------------------------
# Target: tetris_meta (HTTP + SQLite metadata/leaderboard server)
#
# 역할: 별도 기기(Mac mini 등)에서 돌아가는 독립 서비스.
#       · SQLite 로 player/match/elo_history 영속화
#       · cpp-httplib 로 4개 엔드포인트 제공:
#           POST /v1/guest, POST /v1/auth/verify,
#           POST /v1/matches, GET  /v1/leaderboard
#       · relay 는 무상태 유지 — matchmaking 경로에서 HTTP 호출만 붙인다.
#
# 서드파티: third_party/sqlite3.{c,h} + third_party/httplib.h (헤더 온리).
#           두 파일 모두 벤더링(check-in)되어 있어야 한다 — repo 루트의
#           third_party/ 에 없으면 CMake 가 즉시 실패한다.
# -----------------------------------------------------------------------------
if (TETRIS_BUILD_META)
    if (NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/sqlite3.c" OR
        NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/sqlite3.h")
        message(FATAL_ERROR
            "TETRIS_BUILD_META=ON 이지만 third_party/sqlite3.{c,h} 가 없습니다. "
            "SQLite amalgamation 을 다운로드해 third_party/ 에 넣으세요 "
            "(https://www.sqlite.org/download.html).")
    endif()
    if (NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/httplib.h")
        message(FATAL_ERROR
            "TETRIS_BUILD_META=ON 이지만 third_party/httplib.h 가 없습니다. "
            "cpp-httplib single header 를 다운로드해 third_party/ 에 넣으세요 "
            "(https://github.com/yhirose/cpp-httplib).")
    endif()

    add_executable(tetris_meta
        meta/main.cpp
        meta/database.cpp
        meta/api_server.cpp
        third_party/sqlite3.c
        meta/database.h
        meta/api_server.h
        meta/elo.h
        meta/protocol.h
    )
    target_include_directories(tetris_meta PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party
    )
    # SQLite amalgamation — 기본 threadsafe(serialized) 모드로 컴파일.
    # WAL + mutex 는 C++ 래퍼에서 보강한다.
    target_compile_definitions(tetris_meta PRIVATE
        SQLITE_THREADSAFE=1
        SQLITE_ENABLE_RTREE=0
        SQLITE_DEFAULT_FOREIGN_KEYS=1
    )
    if (WIN32)
        target_link_libraries(tetris_meta PRIVATE ws2_32)
    else()
        find_package(Threads REQUIRED)
        target_link_libraries(tetris_meta PRIVATE Threads::Threads ${CMAKE_DL_LIBS})
    endif()
    if (UNIX AND NOT APPLE)
        set_target_properties(tetris_meta PROPERTIES
            BUILD_RPATH "$ORIGIN/lib"
            INSTALL_RPATH "$ORIGIN/lib")
    endif()
endif()
