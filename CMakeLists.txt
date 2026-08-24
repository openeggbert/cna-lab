cmake_minimum_required(VERSION 3.20)

project(wolf_cna LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CNA_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../cna"
    CACHE PATH "Path to the CNA checkout")

if(NOT EXISTS "${CNA_ROOT}/CMakeLists.txt")
    message(FATAL_ERROR
        "wolf-cna: CNA checkout not found at '${CNA_ROOT}'. "
        "Set -DCNA_ROOT=/path/to/cna.")
endif()

if(NOT DEFINED CNA_GRAPHICS_RENDERER)
    if(EMSCRIPTEN)
        set(CNA_GRAPHICS_RENDERER "WEBGL2" CACHE STRING
            "CNA renderer used by wolf-cna")
    else()
        set(CNA_GRAPHICS_RENDERER "OPENGLES3" CACHE STRING
            "CNA renderer used by wolf-cna")
    endif()
endif()

set(CNA_BUILD_TESTS OFF CACHE BOOL "Build CNA tests" FORCE)
set(CNA_BUILD_EXAMPLES OFF CACHE BOOL "Build CNA examples" FORCE)
set(CNA_ENABLE_NET OFF CACHE BOOL "Build CNA networking" FORCE)
set(CNA_ENABLE_VIDEO OFF CACHE STRING "Enable CNA video" FORCE)
set(SHARP_RUNTIME_BUILD_TESTS OFF CACHE BOOL "Build sharp-runtime tests" FORCE)
set(EASYGL_BUILD_TESTS OFF CACHE BOOL "Build EasyGL tests" FORCE)
set(EASYGL_BUILD_EXAMPLES OFF CACHE BOOL "Build EasyGL examples" FORCE)

add_subdirectory("${CNA_ROOT}" "${CMAKE_BINARY_DIR}/cna-build")

add_executable(wolf-cna
    src/CampaignProgress.cpp
    src/LevelDefinition.cpp
    src/Program.cpp
    src/WolfGame.cpp
    src/World.cpp
)

target_include_directories(wolf-cna PRIVATE src)
target_link_libraries(wolf-cna PRIVATE CNA)

configure_file(
    assets/levels/starter.level
    "${CMAKE_CURRENT_BINARY_DIR}/assets/levels/starter.level"
    COPYONLY)
configure_file(
    assets/levels/sector-02.level
    "${CMAKE_CURRENT_BINARY_DIR}/assets/levels/sector-02.level"
    COPYONLY)
configure_file(
    assets/levels/sector-03.level
    "${CMAKE_CURRENT_BINARY_DIR}/assets/levels/sector-03.level"
    COPYONLY)
configure_file(
    assets/sprites/security-guard.png
    "${CMAKE_CURRENT_BINARY_DIR}/assets/sprites/security-guard.png"
    COPYONLY)
configure_file(
    assets/sprites/security-hound.png
    "${CMAKE_CURRENT_BINARY_DIR}/assets/sprites/security-hound.png"
    COPYONLY)
configure_file(
    assets/title/title-background.png
    "${CMAKE_CURRENT_BINARY_DIR}/assets/title/title-background.png"
    COPYONLY)

enable_testing()
add_executable(level-definition-tests
    src/CampaignProgress.cpp
    src/LevelDefinition.cpp
    src/World.cpp
    tests/LevelDefinitionTests.cpp
)
target_include_directories(level-definition-tests PRIVATE src)
target_link_libraries(level-definition-tests PRIVATE CNA)
add_test(NAME level-definition-tests COMMAND level-definition-tests)
set_tests_properties(level-definition-tests PROPERTIES
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")

if(TARGET SDL3::SDL3main)
    target_link_libraries(wolf-cna PRIVATE SDL3::SDL3main)
endif()

if(WIN32)
    set_target_properties(wolf-cna PROPERTIES WIN32_EXECUTABLE TRUE)
    if(COMMAND cna_copy_sdl_runtime)
        cna_copy_sdl_runtime(wolf-cna)
    endif()
endif()
