cmake_minimum_required(VERSION 3.25...3.31)
project(Explore2D VERSION 0.1.0 LANGUAGES CXX)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
    message(FATAL_ERROR "In-source builds are unsupported; use cmake -S . -B build")
endif()

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(EXPLORE2D_BUILD_TESTS "Build Explore2D core tests" ON)
option(EXPLORE2D_BUILD_CNA "Build the CNA host layer when CNA is available" ON)
set(CNA_SOURCE_DIR "" CACHE PATH "Path to a CNA source checkout")

add_library(explore2d_core
    src/Canvas.cpp
    src/Drawing.cpp
    src/Localization.cpp
    src/Persistence.cpp
    src/QBasicSound.cpp
    src/Renderer.cpp
    src/Session.cpp
    src/World.cpp
)
add_library(Explore2D::Core ALIAS explore2d_core)
target_include_directories(explore2d_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(explore2d_core PUBLIC cxx_std_23)

if(MSVC)
    target_compile_options(explore2d_core PRIVATE /W4 /permissive-)
else()
    target_compile_options(explore2d_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(EXPLORE2D_BUILD_CNA)
    if(NOT TARGET CNA AND NOT CNA_SOURCE_DIR STREQUAL "")
        get_filename_component(_explore2d_cna_source "${CNA_SOURCE_DIR}" ABSOLUTE BASE_DIR "${CMAKE_SOURCE_DIR}")
        if(NOT EXISTS "${_explore2d_cna_source}/CMakeLists.txt")
            message(FATAL_ERROR "CNA_SOURCE_DIR does not contain CMakeLists.txt: ${_explore2d_cna_source}")
        endif()
        foreach(option_name IN ITEMS CNA_BUILD_EXAMPLES CNA_BUILD_TESTS CNA_ENABLE_NET)
            set(${option_name} OFF CACHE BOOL "Disabled for embedded Explore2D CNA" FORCE)
        endforeach()
        set(CNA_SDL_PREBUILT_ROOT "${CMAKE_BINARY_DIR}/_deps/cna-sdl-prebuilt" CACHE PATH
            "Out-of-source SDL build/install cache for Explore2D" FORCE)
        set(CNA_GRAPHICS_RENDERER SDL_RENDERER CACHE STRING "Explore2D CNA renderer" FORCE)
        set(CNA_GRAPHICS_BACKEND SDL_RENDERER CACHE STRING "Compatibility spelling" FORCE)
        set(CNA_RENDERER_SDL_RENDERER ON CACHE BOOL "Enable SDL_Renderer" FORCE)
        set(CNA_BACKEND_SDL_RENDERER ON CACHE BOOL "Enable SDL_Renderer" FORCE)
        add_subdirectory("${_explore2d_cna_source}" "${CMAKE_BINARY_DIR}/_deps/cna" EXCLUDE_FROM_ALL)
        # CNA's project(CNA) may create a same-named cache entry. Restore the
        # consumer-facing path after configuring the embedded project.
        set(CNA_SOURCE_DIR "${_explore2d_cna_source}" CACHE PATH "Path to a CNA source checkout" FORCE)
    endif()

    if(TARGET CNA)
        add_library(explore2d_cna src/CnaGame.cpp)
        add_library(Explore2D::Cna ALIAS explore2d_cna)
        target_include_directories(explore2d_cna PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
        target_link_libraries(explore2d_cna PUBLIC Explore2D::Core CNA)
        target_compile_features(explore2d_cna PUBLIC cxx_std_23)
        if(MSVC)
            target_compile_options(explore2d_cna PRIVATE /W4 /permissive-)
        else()
            target_compile_options(explore2d_cna PRIVATE -Wall -Wextra -Wpedantic)
        endif()
    else()
        message(STATUS "CNA target unavailable: building Explore2D::Core only")
    endif()
endif()

if(EXPLORE2D_BUILD_TESTS)
    enable_testing()
    add_executable(explore2d_core_tests test/CoreTests.cpp)
    target_link_libraries(explore2d_core_tests PRIVATE Explore2D::Core)
    add_test(NAME explore2d.core COMMAND explore2d_core_tests)
endif()
