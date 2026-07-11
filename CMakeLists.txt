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
    src/CnaCraft/Worlds/DayNightCycle.cpp
    src/CnaCraft/Worlds/Sign.cpp
    src/CnaCraft/Worlds/WorldEditor.cpp
)
target_include_directories(CnaCraftWorlds PUBLIC src)

# =============================================================================
# CnaCraftNetProtocol — the multiplayer wire protocol (MULTIPLAYER_PLAN.md
# §2/§5, plan.md §12.1 item 18). Pure parse/format over std:: only -- no
# sockets, no sharp-runtime, no CNA -- so it builds and tests in the
# worlds-only configuration exactly like CnaCraftWorlds. The socket-using
# transport (Net/GameClient) is deliberately NOT in this library: it needs
# SHARP_RUNTIME, which only exists when the game build pulls in the CNA tree.
# =============================================================================
add_library(CnaCraftNetProtocol STATIC
    src/CnaCraft/Net/Protocol.cpp
)
target_include_directories(CnaCraftNetProtocol PUBLIC src)

if(BUILD_TESTING)
    add_executable(cna_craft_worlds_smoke_test tests/worlds_smoke_test.cpp)
    target_link_libraries(cna_craft_worlds_smoke_test PRIVATE CnaCraftWorlds)
    add_test(NAME WorldsSmokeTest COMMAND cna_craft_worlds_smoke_test)

    add_executable(cna_craft_net_protocol_smoke_test tests/net_protocol_smoke_test.cpp)
    target_link_libraries(cna_craft_net_protocol_smoke_test PRIVATE CnaCraftNetProtocol)
    add_test(NAME NetProtocolSmokeTest COMMAND cna_craft_net_protocol_smoke_test)

    # Persistence::WorldStore (CRAFT_PARITY.md §4.1/§4.2, plan.md §12.1 item
    # 15) needs real SQLite + filesystem access but, like CnaCraftWorlds
    # itself, no CNA/GPU/display -- a separate small test target/ctest, not
    # bundled into cna_craft_worlds_smoke_test, since it's the only part of
    # this repo's test suite that touches disk I/O. Degrades gracefully
    # (skipped, not a fatal configure error) if SQLite3 isn't available,
    # mirroring how CNA_CRAFT_BUILD_GAME degrades when CNA isn't found.
    find_package(SQLite3 QUIET)
    if(SQLite3_FOUND)
        add_executable(cna_craft_persistence_smoke_test
            tests/persistence_smoke_test.cpp
            src/CnaCraft/Persistence/WorldStore.cpp
        )
        target_include_directories(cna_craft_persistence_smoke_test PRIVATE src)
        target_link_libraries(cna_craft_persistence_smoke_test PRIVATE CnaCraftWorlds SQLite::SQLite3)
        add_test(NAME PersistenceSmokeTest COMMAND cna_craft_persistence_smoke_test)
    else()
        message(STATUS "CnaCraft: SQLite3 not found -- skipping cna_craft_persistence_smoke_test")
    endif()
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

    # World persistence (CRAFT_PARITY.md §4.1/§4.2, plan.md §12.1 item 15 --
    # user decision 2026-07-10: add SQLite). Only needed by the CnaCraft
    # executable's Persistence/ layer, not by the engine-agnostic
    # CnaCraftWorlds library above -- that library's whole point is staying
    # dependency-free and testable with no GPU/display, and persistence is a
    # new, separate concern from core voxel logic (see Persistence/WorldStore).
    find_package(SQLite3 REQUIRED)

    add_executable(CnaCraft
        src/CnaCraft/main.cpp
        src/CnaCraft/CnaCraftGame.cpp
        src/CnaCraft/Render/TextureAtlas.cpp
        src/CnaCraft/Render/ChunkRenderer.cpp
        src/CnaCraft/Render/BitmapFont.cpp
        src/CnaCraft/Render/Hud.cpp
        src/CnaCraft/Render/SelectionOutline.cpp
        src/CnaCraft/Render/SkyDome.cpp
        src/CnaCraft/Render/SkyTexture.cpp
        src/CnaCraft/Render/SignBillboard.cpp
        src/CnaCraft/Persistence/WorldStore.cpp
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
    target_link_libraries(CnaCraft PRIVATE SQLite::SQLite3)
    if(TARGET SDL3::SDL3main)
        target_link_libraries(CnaCraft PRIVATE SDL3::SDL3main)
    endif()
    if(WIN32 AND COMMAND cna_copy_sdl_runtime)
        set_target_properties(CnaCraft PROPERTIES WIN32_EXECUTABLE TRUE)
        cna_copy_sdl_runtime(CnaCraft)
    endif()
endif()
