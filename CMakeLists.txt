# CMAKE 최소 버전
cmake_minimum_required(VERSION 3.10)

# 프로젝트 이름 및 언어 설정
project(TetrisMultiplayer CXX)

# C++ 표준 설정
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 소스 파일 설정
set(SERVER_SOURCES
    src/main_server.cpp
    src/server.cpp
    src/network_protocol.cpp
    src/player.cpp
    src/game.cpp
    src/grid.cpp
    src/block.cpp
    src/position.cpp
    src/ai_bot.cpp
    src/ai_player.cpp
    src/game_mode.cpp
)

set(CLIENT_SOURCES
    src/main_client.cpp
    src/client.cpp
    src/network_protocol.cpp
    src/game.cpp
    src/grid.cpp
    src/block.cpp
    src/position.cpp
    src/ai_bot.cpp
    src/ai_player.cpp
    src/game_mode.cpp
)

# 헤더 파일 경로 설정
include_directories(include)

# ASIO 라이브러리 설정 (헤더 전용 라이브러리)
add_definitions(-DASIO_STANDALONE)
include_directories(${CMAKE_SOURCE_DIR}/external/include)

# ncurses 라이브러리 찾기
find_package(Curses REQUIRED)
include_directories(${CURSES_INCLUDE_DIR})

# 서버 실행 파일 설정
add_executable(tetris_server ${SERVER_SOURCES})
target_link_libraries(tetris_server ${CURSES_LIBRARIES} pthread)

# 클라이언트 실행 파일 설정
add_executable(tetris_client ${CLIENT_SOURCES})
target_link_libraries(tetris_client ${CURSES_LIBRARIES} pthread)

# 설치 설정
install(TARGETS tetris_server tetris_client
        RUNTIME DESTINATION bin)