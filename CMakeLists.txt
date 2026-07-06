cmake_minimum_required(VERSION 3.21)
project(CnaCraft LANGUAGES C CXX)

include(CTest)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

message(STATUS "Project: ${PROJECT_NAME}")
message(STATUS "Platform: ${CMAKE_SYSTEM_NAME}")
message(STATUS "Compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")

# =============================================================================
# CnaCraftWorlds — engine-agnostic voxel core (no CNA/SDL dependency).
#
# Chunk/World/NoiseGenerator/ChunkMesher/VoxelRaycast/PlayerController: pure
# C++23, no GPU or windowing types (see plan.md §2/§8). Builds and tests in
# any environment, including one without a GPU or display.
# =============================================================================
add_library(CnaCraftWorlds STATIC
    src/CnaCraft/Worlds/Chunk.cpp
    src/CnaCraft/Worlds/World.cpp
    src/CnaCraft/Worlds/NoiseGenerator.cpp
    src/CnaCraft/Worlds/ChunkMesher.cpp
    src/CnaCraft/Worlds/VoxelRaycast.cpp
    src/CnaCraft/Worlds/PlayerController.cpp
    src/CnaCraft/Worlds/Hotbar.cpp
)
target_include_directories(CnaCraftWorlds PUBLIC src)

if(BUILD_TESTING)
    add_executable(cna_craft_worlds_smoke_test tests/worlds_smoke_test.cpp)
    target_link_libraries(cna_craft_worlds_smoke_test PRIVATE CnaCraftWorlds)
    add_test(NAME WorldsSmokeTest COMMAND cna_craft_worlds_smoke_test)
endif()

# =============================================================================
# CnaCraft — graphical executable (CNA-dependent).
#
# Build with one of the three 3D-capable backends:
#   cmake -S . -B build -DCNA_GRAPHICS_BACKEND=EASYGL   # or VULKAN, or BGFX
#   cmake --build build --target CnaCraft
#
# Requires ../cna and ../sharp-runtime checked out as siblings of this repo
# (see README.md §3), following the same convention as
# ../galaxy-eggbert/CMakeLists.txt's GALAXY_EGGBERT_BUILD_CNA option. Unlike
# that option, this one degrades to a warning (not a fatal error) when CNA
# isn't found, so `cmake -S . -B build` with no arguments always succeeds in
# building at least CnaCraftWorlds + its smoke test.
# =============================================================================
option(CNA_CRAFT_BUILD_GAME "Build the graphical CnaCraft executable (requires ../cna and ../sharp-runtime)" ON)

if(CNA_CRAFT_BUILD_GAME)
    set(CNA_HOME "" CACHE PATH "Path to the CNA source directory. Defaults to ../cna.")
    if(NOT CNA_HOME)
        set(CNA_HOME "${CMAKE_CURRENT_SOURCE_DIR}/../cna")
    endif()

    if(NOT EXISTS "${CNA_HOME}/CMakeLists.txt")
        message(WARNING
            "CnaCraft: CNA not found at '${CNA_HOME}' -- skipping the graphical CnaCraft "
            "target (CnaCraftWorlds + its smoke test still build). Clone CNA there, or pass "
            "-DCNA_HOME=<path>, to build the full game.")
        set(CNA_CRAFT_BUILD_GAME OFF)
    endif()
endif()

if(CNA_CRAFT_BUILD_GAME)
    # 3D rendering is implemented on EASYGL, VULKAN, and BGFX (pick any one at
    # configure time); SDL_RENDERER remains 2D-only as of this writing (see
    # ../cna/examples/house3d_demo.cpp and analysis.md).
    set(CNA_CRAFT_3D_BACKENDS EASYGL VULKAN BGFX)
    if(NOT CNA_GRAPHICS_BACKEND)
        set(CNA_GRAPHICS_BACKEND "EASYGL" CACHE STRING
            "Graphics backend CnaCraft is built against (EASYGL, VULKAN, or BGFX)." FORCE)
    endif()
    set_property(CACHE CNA_GRAPHICS_BACKEND PROPERTY STRINGS ${CNA_CRAFT_3D_BACKENDS} SDL_RENDERER)
    if(NOT CNA_GRAPHICS_BACKEND IN_LIST CNA_CRAFT_3D_BACKENDS)
        message(WARNING
            "CnaCraft: 3D rendering requires one of ${CNA_CRAFT_3D_BACKENDS}; "
            "CNA_GRAPHICS_BACKEND is currently '${CNA_GRAPHICS_BACKEND}'.")
    endif()

    message(STATUS "CNA source: ${CNA_HOME}")
    message(STATUS "CnaCraft graphics backend: ${CNA_GRAPHICS_BACKEND}")

    set(CNA_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(CNA_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    if(NOT TARGET CNA)
        # CNA's own CMakeLists.txt pulls in ../sharp-runtime (relative to
        # CNA_HOME) and defines the CNA + backend + SHARP_RUNTIME targets.
        add_subdirectory("${CNA_HOME}" CNA_dep)
    endif()

    add_executable(CnaCraft
        src/CnaCraft/main.cpp
        src/CnaCraft/CnaCraftGame.cpp
        src/CnaCraft/Render/TextureAtlas.cpp
        src/CnaCraft/Render/ChunkRenderer.cpp
    )
    target_include_directories(CnaCraft PRIVATE src)

    set(_cna_craft_backend_target "")
    if(CNA_GRAPHICS_BACKEND STREQUAL "SDL_RENDERER")
        set(_cna_craft_backend_target "cna_backend_graphics_sdl_renderer")
    elseif(CNA_GRAPHICS_BACKEND STREQUAL "EASYGL")
        set(_cna_craft_backend_target "cna_backend_graphics_easygl")
    elseif(CNA_GRAPHICS_BACKEND STREQUAL "BGFX")
        set(_cna_craft_backend_target "cna_backend_graphics_bgfx")
    elseif(CNA_GRAPHICS_BACKEND STREQUAL "VULKAN")
        set(_cna_craft_backend_target "cna_backend_graphics_vulkan")
    endif()

    # CNA and its backend static library have circular symbol references
    # (mirrors ../cna/CMakeLists.txt's own examples and
    # ../galaxy-eggbert/CMakeLists.txt's GalaxyEggbertCNA target).
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang" AND NOT WIN32 AND _cna_craft_backend_target)
        target_link_libraries(CnaCraft PRIVATE
            -Wl,--start-group CNA ${_cna_craft_backend_target} -Wl,--end-group
            SHARP_RUNTIME CnaCraftWorlds)
    else()
        target_link_libraries(CnaCraft PRIVATE CNA SHARP_RUNTIME CnaCraftWorlds)
    endif()
    if(TARGET SDL3::SDL3main)
        target_link_libraries(CnaCraft PRIVATE SDL3::SDL3main)
    endif()
    if(WIN32 AND COMMAND cna_copy_sdl_runtime)
        set_target_properties(CnaCraft PROPERTIES WIN32_EXECUTABLE TRUE)
        cna_copy_sdl_runtime(CnaCraft)
    endif()
endif()
