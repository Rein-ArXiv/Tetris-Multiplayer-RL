cmake_minimum_required(VERSION 3.15)
project(tetris CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Sources
file(GLOB SRC_FILES CONFIGURE_DEPENDS
    src/*.cpp
)
# Exclude blocks.cpp (included via headers into game.h)
list(REMOVE_ITEM SRC_FILES "${CMAKE_CURRENT_SOURCE_DIR}/src/blocks.cpp")

add_executable(tetris ${SRC_FILES}
    core/constants.h
    core/input.h
    core/rng.h
    core/hash.h
    core/replay.h
    core/replay.cpp
    net/socket.h
    net/socket.cpp
    net/framing.h
    net/framing.cpp
    net/session.h
    net/session.cpp
)

target_include_directories(tetris PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

if (WIN32)
    # Default raylib root; adjust via -DRAYLIB_ROOT=C:/path/to/raylib
    set(RAYLIB_ROOT "C:/raylib/raylib" CACHE PATH "Path to raylib on Windows")
    target_include_directories(tetris PRIVATE ${RAYLIB_ROOT}/src)
    target_link_directories(tetris PRIVATE ${RAYLIB_ROOT}/src ${RAYLIB_ROOT}/release/libs/win64)
    target_link_libraries(tetris PRIVATE raylib opengl32 gdi32 winmm ws2_32)
else()
    # Try pkg-config first
    find_package(PkgConfig QUIET)
    if (PkgConfig_FOUND)
        pkg_check_modules(RAYLIB QUIET raylib)
    endif()
    if (RAYLIB_FOUND)
        target_include_directories(tetris PRIVATE ${RAYLIB_INCLUDE_DIRS})
        target_link_directories(tetris PRIVATE ${RAYLIB_LIBRARY_DIRS})
        target_link_libraries(tetris PRIVATE ${RAYLIB_LIBRARIES})
    else()
        message(STATUS "raylib via pkg-config not found; falling back to /usr/local")
        target_include_directories(tetris PRIVATE /usr/local/include)
        target_link_directories(tetris PRIVATE /usr/local/lib)
        target_link_libraries(tetris PRIVATE raylib)
    endif()
    # Common Linux libs when linking statically; keep dynamic default
    find_package(Threads REQUIRED)
    target_link_libraries(tetris PRIVATE m dl ${CMAKE_THREAD_LIBS_INIT})
endif()

# Copy assets on build (optional)
add_custom_target(copy_assets ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/Font ${CMAKE_CURRENT_BINARY_DIR}/Font
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/Sounds ${CMAKE_CURRENT_BINARY_DIR}/Sounds
    DEPENDS tetris
)
