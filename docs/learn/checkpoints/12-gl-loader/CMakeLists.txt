cmake_minimum_required(VERSION 3.15)
project(tetris_study LANGUAGES CXX)
set(CMAKE_CXX_EXTENSIONS OFF)

set(STUDY_PLATFORM "SDL" CACHE STRING "Platform implementation: SDL, SCRIPTED, or WIN32")
set_property(CACHE STUDY_PLATFORM PROPERTY STRINGS SDL SCRIPTED WIN32)

if(STUDY_PLATFORM STREQUAL "SDL")
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
    set(platform_source platform/sdl.cpp)
elseif(STUDY_PLATFORM STREQUAL "SCRIPTED")
    set(platform_source platform/scripted.cpp)
elseif(STUDY_PLATFORM STREQUAL "WIN32")
    # CMake's WIN32 variable means "target system is Windows", not CPU bitness.
    # The Win32 backend needs windows.h and user32, so it cannot be configured
    # on a non-Windows target.
    if(NOT WIN32)
        message(FATAL_ERROR
            "STUDY_PLATFORM=WIN32 requires a Windows target. "
            "CMake's WIN32 variable describes the target system, not CPU bitness; "
            "use a Windows compiler/toolchain.")
    endif()
    set(platform_source platform/win32.cpp)
else()
    message(FATAL_ERROR "Unknown STUDY_PLATFORM=${STUDY_PLATFORM}; use SDL, SCRIPTED, or WIN32")
endif()

# Choose exactly one implementation of the common functions.
add_library(study_platform STATIC ${platform_source})
target_compile_features(study_platform PUBLIC cxx_std_17)
target_include_directories(study_platform PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
if(STUDY_PLATFORM STREQUAL "SDL")
    target_link_libraries(study_platform PRIVATE StudySDL2)
elseif(STUDY_PLATFORM STREQUAL "WIN32")
    # user32 only: no GL, mouse, audio, or DPI dependencies yet.
    target_link_libraries(study_platform PRIVATE user32)
endif()

# No WIN32 keyword on add_executable: this stays a console program with main().
add_library(study_gl STATIC renderer/gl_api.cpp)
target_compile_features(study_gl PUBLIC cxx_std_17)
target_include_directories(study_gl PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

# Input comparison remains available for all teaching backends.
add_executable(input_demo src/input_demo.cpp)
target_link_libraries(input_demo PRIVATE study_platform)
if(STUDY_PLATFORM STREQUAL "SDL")
    add_executable(tetris src/main.cpp)
    target_link_libraries(tetris PRIVATE study_platform study_gl)
else()
    message(STATUS "GL demo requires SDL; this backend provides input_demo only")
endif()

include(CTest)
if(BUILD_TESTING AND STUDY_PLATFORM STREQUAL "WIN32")
    add_executable(platform_contract tests/contracts.cpp)
    target_link_libraries(platform_contract PRIVATE study_platform user32)
    add_test(NAME win32_contract COMMAND platform_contract)
endif()

if(BUILD_TESTING)
    add_executable(loader_contract tests/loader_contract.cpp)
    target_link_libraries(loader_contract PRIVATE study_gl)
    add_test(NAME loader_contract COMMAND loader_contract)
endif()
