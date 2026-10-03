cmake_minimum_required(VERSION 3.15)
project(tetris_study LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Prefer SDL2 CMake packages; pkg-config is a fallback for system packages.
add_library(StudySDL2 INTERFACE)
# Keep the console entry point; SDL readiness is explicit before SDL_Init.
target_compile_definitions(StudySDL2 INTERFACE SDL_MAIN_HANDLED)
find_package(SDL2 CONFIG QUIET)
if(TARGET SDL2::SDL2)
    target_link_libraries(StudySDL2 INTERFACE SDL2::SDL2)
elseif(TARGET SDL2::SDL2-static)
    target_link_libraries(StudySDL2 INTERFACE SDL2::SDL2-static)
else()
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(SDL2 REQUIRED IMPORTED_TARGET sdl2)
    target_link_libraries(StudySDL2 INTERFACE PkgConfig::SDL2)
endif()

add_executable(tetris src/main.cpp)
target_link_libraries(tetris PRIVATE StudySDL2)
