cmake_minimum_required(VERSION 3.23)

project(CopperBoots VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(COPPER_BOOTS_BUILD_TESTS "Build Copper Boots logic tests" ON)
option(COPPER_BOOTS_USE_CCACHE
    "Allow CNA to configure ccache for this consumer build" OFF)

if(NOT COPPER_BOOTS_USE_CCACHE)
    set(CMAKE_C_COMPILER_LAUNCHER "" CACHE STRING "C compiler launcher" FORCE)
    set(CMAKE_CXX_COMPILER_LAUNCHER "" CACHE STRING "C++ compiler launcher" FORCE)
endif()

set(CNA_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../cna" CACHE PATH
    "Path to the CNA framework checkout")

if(NOT EXISTS "${CNA_ROOT_DIR}/CMakeLists.txt")
    message(FATAL_ERROR
        "Copper Boots: CNA was not found at '${CNA_ROOT_DIR}'. "
        "Place CNA beside mario-cna or pass -DCNA_ROOT_DIR=/path/to/cna.")
endif()

# CNA currently interprets a consumer's root-level src/include directories as
# its own forbidden legacy layout (MAR-CNA-001). Keep our code under game/ and
# gameplay/ until the upstream validator uses CNA's own project root.
foreach(_reserved IN ITEMS src include)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${_reserved}")
        message(FATAL_ERROR
            "Copper Boots: root '${_reserved}/' triggers CNA MAR-CNA-001; "
            "use game/ or gameplay/ until the pinned CNA revision is fixed.")
    endif()
endforeach()

if(NOT DEFINED CNA_GRAPHICS_RENDERER OR CNA_GRAPHICS_RENDERER STREQUAL "")
    set(CNA_GRAPHICS_RENDERER "SDL_RENDERER" CACHE STRING
        "CNA graphics renderer" FORCE)
endif()

set(CNA_BUILD_TESTS OFF CACHE BOOL "Build CNA tests" FORCE)
set(CNA_BUILD_EXAMPLES OFF CACHE BOOL "Build CNA examples" FORCE)
set(CNA_ENABLE_NET OFF CACHE BOOL "Build CNA networking" FORCE)
set(CNA_USE_CCACHE ${COPPER_BOOTS_USE_CCACHE} CACHE BOOL
    "Use CNA ccache integration" FORCE)
set(SHARP_RUNTIME_BUILD_TESTS OFF CACHE BOOL "Build sharp-runtime tests" FORCE)
set(EASYGL_BUILD_TESTS OFF CACHE BOOL "Build easy-gl tests" FORCE)
set(EASYGL_BUILD_EXAMPLES OFF CACHE BOOL "Build easy-gl examples" FORCE)

if(NOT DEFINED CNA_SDL_PREBUILT_ROOT)
    set(CNA_SDL_PREBUILT_ROOT
        "${CMAKE_CURRENT_BINARY_DIR}/cna-sdl-prebuilt"
        CACHE PATH "Persistent CNA SDL dependency cache")
endif()

# Load CNA's own narrow sharp-runtime component closure before CNA adds the
# sibling runtime. This avoids accidentally instantiating SharpRuntime::All.
set(_cna_sharp_runtime_consumption
    "${CNA_ROOT_DIR}/cmake/SharpRuntimeConsumption.cmake")
if(EXISTS "${_cna_sharp_runtime_consumption}")
    include("${_cna_sharp_runtime_consumption}")
    if(NOT DEFINED SHARP_RUNTIME_COMPONENTS)
        set(SHARP_RUNTIME_COMPONENTS "${CNA_SHARP_RUNTIME_DEFAULT_COMPONENTS}"
            CACHE STRING "Sharp Runtime components required by CNA")
    endif()
endif()

# Narrow workaround for CNA MAR-CNA-002: these vendored include roots are
# otherwise resolved relative to this consumer by the pinned CNA revision.
foreach(_vendored_header IN ITEMS cgltf stb)
    if(EXISTS "${CNA_ROOT_DIR}/third_party/${_vendored_header}")
        include_directories("${CNA_ROOT_DIR}/third_party/${_vendored_header}")
    endif()
endforeach()

add_subdirectory("${CNA_ROOT_DIR}" CNA)

add_library(copper_boots_gameplay STATIC
    gameplay/src/Camera2D.cpp
    gameplay/src/SimulationClock.cpp
    gameplay/src/TileMap.cpp
    gameplay/src/WorldSimulation.cpp
)
target_include_directories(copper_boots_gameplay PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/gameplay/include")
target_compile_features(copper_boots_gameplay PUBLIC cxx_std_23)

add_executable(copper-boots
    game/src/CopperBootsGame.cpp
    game/src/Program.cpp
)
target_include_directories(copper-boots PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/game/include")
target_link_libraries(copper-boots PRIVATE copper_boots_gameplay CNA)

if(TARGET SDL3::SDL3main)
    target_link_libraries(copper-boots PRIVATE SDL3::SDL3main)
endif()

if(COMMAND cna_copy_renderer_runtime)
    cna_copy_renderer_runtime(copper-boots)
endif()

if(MSVC)
    target_compile_options(copper_boots_gameplay PRIVATE /W4 /WX)
    target_compile_options(copper-boots PRIVATE /W4 /WX)
else()
    target_compile_options(copper_boots_gameplay PRIVATE
        -Wall -Wextra -Werror)
    target_compile_options(copper-boots PRIVATE
        -Wall -Wextra -Werror)
endif()

if(COPPER_BOOTS_BUILD_TESTS)
    enable_testing()
    add_executable(copper_boots_gameplay_tests
        test/GameplayTests.cpp)
    target_link_libraries(copper_boots_gameplay_tests PRIVATE
        copper_boots_gameplay)
    if(MSVC)
        target_compile_options(copper_boots_gameplay_tests PRIVATE /W4 /WX)
    else()
        target_compile_options(copper_boots_gameplay_tests PRIVATE
            -Wall -Wextra -Werror)
    endif()
    add_test(NAME copper_boots_gameplay COMMAND copper_boots_gameplay_tests)

    if(NOT CMAKE_CROSSCOMPILING AND NOT EMSCRIPTEN)
        add_test(NAME copper_boots_smoke COMMAND copper-boots --smoke-test)
        set_tests_properties(copper_boots_smoke PROPERTIES TIMEOUT 20)
        if(UNIX AND NOT APPLE)
            set_tests_properties(copper_boots_smoke PROPERTIES
                ENVIRONMENT "SDL_VIDEODRIVER=offscreen;SDL_AUDIODRIVER=dummy")
        endif()
    endif()
endif()
