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
    src/CnaCraft/Worlds/RemotePlayer.cpp
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

# Test binaries are host-run (ctest executes them directly), so they are
# skipped when cross-compiling to wasm -- run the test suites in a native
# build tree instead (the web build changes no engine-agnostic logic).
if(BUILD_TESTING AND NOT EMSCRIPTEN)
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
# Build with one of the three 3D-capable renderers:
#   cmake -S . -B build -DCNA_GRAPHICS_RENDERER=OPENGLES3   # or VULKAN, or BGFX
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

# Wasm threads (plan.md §12.1 item 37): -pthread must reach EVERY object --
# game, CnaCraftWorlds, CNA, easy-gl, sharp-runtime, SDL -- or wasm-ld refuses
# to link ("--shared-memory is disallowed by X.o because it was not compiled
# with 'atomics'"). Setting it here, BEFORE add_subdirectory(CNA), covers
# everything built in this tree; CNA's SDL prebuilt is a separate cmake
# invocation that inherits only the ENVIRONMENT, which is why the documented
# configure line also exports CFLAGS/CXXFLAGS (see README §4.1). Opt out with
# -DCNA_CRAFT_WASM_THREADS=OFF for hosting that cannot send COOP/COEP headers;
# the game then falls back to synchronous generation/meshing on the main
# thread (visibly stuttery while terrain streams -- that is exactly the
# regression item 37 fixed).
if(EMSCRIPTEN)
    option(CNA_CRAFT_WASM_THREADS "Build the web target with Emscripten pthreads (needs COOP/COEP hosting)" ON)
    if(CNA_CRAFT_WASM_THREADS)
        add_compile_options(-pthread)
        add_link_options(-pthread)
    endif()
endif()

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
    # 3D rendering is implemented on the GL family (via CNA's shared EasyGL
    # implementation, publicly selected as OPENGLES3/OPENGLES2/OPENGL33 on
    # native builds or WEBGL1/WEBGL2 under Emscripten -- "EASYGL" itself is no
    # longer a public CNA_GRAPHICS_RENDERER value, see ../cna/cmake/
    # RendererSelection.cmake), VULKAN, and BGFX (pick any one at configure
    # time); SDL_RENDERER remains 2D-only as of this writing (see
    # ../cna/examples/house3d_demo.cpp and analysis.md).
    if(EMSCRIPTEN)
        set(CNA_CRAFT_3D_RENDERERS WEBGL1 WEBGL2 VULKAN BGFX)
        set(_cna_craft_default_renderer "WEBGL2")
    else()
        set(CNA_CRAFT_3D_RENDERERS OPENGLES2 OPENGLES3 OPENGL33 VULKAN BGFX)
        set(_cna_craft_default_renderer "OPENGLES3")
    endif()
    if(NOT CNA_GRAPHICS_RENDERER)
        set(CNA_GRAPHICS_RENDERER "${_cna_craft_default_renderer}" CACHE STRING
            "Graphics renderer CnaCraft is built against (OPENGLES3, OPENGLES2, OPENGL33, WEBGL1, WEBGL2, VULKAN, or BGFX)." FORCE)
    endif()
    set_property(CACHE CNA_GRAPHICS_RENDERER PROPERTY STRINGS ${CNA_CRAFT_3D_RENDERERS} SDL_RENDERER)
    if(NOT CNA_GRAPHICS_RENDERER IN_LIST CNA_CRAFT_3D_RENDERERS)
        message(WARNING
            "CnaCraft: 3D rendering requires one of ${CNA_CRAFT_3D_RENDERERS}; "
            "CNA_GRAPHICS_RENDERER is currently '${CNA_GRAPHICS_RENDERER}'.")
    endif()

    message(STATUS "CNA source: ${CNA_HOME}")
    message(STATUS "CnaCraft graphics renderer: ${CNA_GRAPHICS_RENDERER}")

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
    #
    # Web build: no SQLite on wasm -- WorldStoreStub.cpp implements the same
    # interface as harmless no-ops (the store's existing open-failure
    # degradation, decided at compile time instead of runtime). Edits then
    # live only for the browser session, which is what a web demo wants
    # anyway.
    if(EMSCRIPTEN)
        set(_cna_craft_persistence_src src/CnaCraft/Persistence/WorldStoreStub.cpp)
    else()
        find_package(SQLite3 REQUIRED)
        set(_cna_craft_persistence_src src/CnaCraft/Persistence/WorldStore.cpp)
    endif()

    # =========================================================================
    # CnaCraftNetTransport -- the socket-using half of Net/ (MULTIPLAYER_PLAN.md
    # §5): LineSocket framing + GameClient. Lives inside the game-enabled
    # branch because it needs SHARP_RUNTIME (System::Net::Sockets), which only
    # exists once the CNA tree is added. Pure protocol parse/format stays in
    # CnaCraftNetProtocol above, testable without any of this.
    # =========================================================================
    add_library(CnaCraftNetTransport STATIC
        src/CnaCraft/Net/LineSocket.cpp
        src/CnaCraft/Net/GameClient.cpp
    )
    target_include_directories(CnaCraftNetTransport PUBLIC src)
    target_link_libraries(CnaCraftNetTransport PUBLIC CnaCraftNetProtocol SHARP_RUNTIME)

    # CnaCraftServer -- the authoritative headless game server
    # (MULTIPLAYER_PLAN.md §6): no CNA/graphics, just Worlds/ + Persistence/
    # + the Net transport. Runs anywhere the worlds tests do -- except wasm,
    # where sharp-runtime's sockets are unsupported (a browser page can't
    # accept TCP connections anyway; the web build is offline single-player).
    if(NOT EMSCRIPTEN)
        add_executable(CnaCraftServer
            src/CnaCraft/Server/main.cpp
            src/CnaCraft/Server/GameServer.cpp
            src/CnaCraft/Persistence/WorldStore.cpp
        )
        target_include_directories(CnaCraftServer PRIVATE src)
        target_link_libraries(CnaCraftServer PRIVATE CnaCraftNetTransport CnaCraftWorlds SQLite::SQLite3)
    endif()

    if(BUILD_TESTING AND NOT EMSCRIPTEN)
        add_executable(cna_craft_net_transport_smoke_test tests/net_transport_smoke_test.cpp)
        target_link_libraries(cna_craft_net_transport_smoke_test PRIVATE CnaCraftNetTransport)
        add_test(NAME NetTransportSmokeTest COMMAND cna_craft_net_transport_smoke_test)

        add_executable(cna_craft_net_server_smoke_test
            tests/net_server_smoke_test.cpp
            src/CnaCraft/Server/GameServer.cpp
            src/CnaCraft/Persistence/WorldStore.cpp
        )
        target_include_directories(cna_craft_net_server_smoke_test PRIVATE src)
        target_link_libraries(cna_craft_net_server_smoke_test PRIVATE CnaCraftNetTransport CnaCraftWorlds
                                                                      SQLite::SQLite3)
        add_test(NAME NetServerSmokeTest COMMAND cna_craft_net_server_smoke_test)

        add_executable(cna_craft_net_worldsync_smoke_test
            tests/net_worldsync_smoke_test.cpp
            src/CnaCraft/Server/GameServer.cpp
            src/CnaCraft/Persistence/WorldStore.cpp
        )
        target_include_directories(cna_craft_net_worldsync_smoke_test PRIVATE src)
        target_link_libraries(cna_craft_net_worldsync_smoke_test PRIVATE CnaCraftNetTransport CnaCraftWorlds
                                                                         SQLite::SQLite3)
        add_test(NAME NetWorldSyncSmokeTest COMMAND cna_craft_net_worldsync_smoke_test)
    endif()

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
        src/CnaCraft/Render/PlayerCube.cpp
        ${_cna_craft_persistence_src}
    )
    target_include_directories(CnaCraft PRIVATE src)

    # Target names per ../cna/cmake/RendererSelection.cmake: the GL-family
    # public renderers (OPENGLES2/OPENGLES3/OPENGL33/WEBGL1/WEBGL2) all share
    # the single cna_renderer_easygl implementation target.
    set(_cna_craft_backend_target "")
    if(CNA_GRAPHICS_RENDERER STREQUAL "SDL_RENDERER")
        set(_cna_craft_backend_target "cna_renderer_sdl_renderer")
    elseif(CNA_GRAPHICS_RENDERER MATCHES "^(OPENGLES2|OPENGLES3|OPENGL33|WEBGL1|WEBGL2)$")
        set(_cna_craft_backend_target "cna_renderer_easygl")
    elseif(CNA_GRAPHICS_RENDERER STREQUAL "BGFX")
        set(_cna_craft_backend_target "cna_renderer_bgfx")
    elseif(CNA_GRAPHICS_RENDERER STREQUAL "VULKAN")
        set(_cna_craft_backend_target "cna_renderer_vulkan")
    endif()

    # CNA and its backend static library have circular symbol references
    # (mirrors ../cna/CMakeLists.txt's own examples and
    # ../galaxy-eggbert/CMakeLists.txt's GalaxyEggbertCNA target).
    if(EMSCRIPTEN)
        # Web build (plan.md §12.1 item 18 follow-up): same target shape as
        # CNA's own verified web demos (../cna/CMakeLists.txt's cna_demo_2d
        # Emscripten branch). No --preload-file: every cna-craft asset
        # (texture atlas, sky gradient, bitmap font) is generated
        # procedurally at startup -- there is no content directory at all.
        set_target_properties(CnaCraft PROPERTIES SUFFIX ".html")
        # -pthread itself is applied tree-wide near the top of this file (it
        # must also reach CNA/easy-gl/sharp-runtime, not just this target).
        # With it, sharp-runtime defines __EMSCRIPTEN_PTHREADS__, its TaskT
        # stops throwing, and the game takes its NORMAL backgrounded
        # generation/meshing path in the browser -- no wasm-specific code.
        # SDL3 (CNA's vendored static libSDL3.a) flows in transitively
        # through the backend target's own link interface, same as the
        # native build below -- naming SDL3::SDL3 here directly would fail
        # at generate time (it's an imported target scoped to the CNA
        # subdirectory).
        target_link_libraries(CnaCraft PRIVATE
            CNA ${_cna_craft_backend_target} SHARP_RUNTIME
            CnaCraftWorlds CnaCraftNetTransport)
        target_link_options(CnaCraft PRIVATE
            # FIXED 512 MB heap, deliberately NO -sALLOW_MEMORY_GROWTH: a
            # growable wasm heap is backed by a RESIZABLE ArrayBuffer, and
            # current browsers (Chrome 130+) reject views of resizable
            # buffers in crypto.getRandomValues AND WebGL's texImage2D/
            # bufferData upload paths -- the latter would mean copying
            # every texture/vertex upload through a temp. A fixed heap
            # sidesteps the whole class; 512 MB comfortably fits the
            # streamed world at the default radius.
            -sINITIAL_MEMORY=536870912
            -sSTACK_SIZE=1048576
            -sFORCE_FILESYSTEM=1
            # EasyGL's shaders are '#version 300 es' -- require a real
            # WebGL 2 context (= OpenGL ES 3.0), never a WebGL 1 fallback.
            -sMAX_WEBGL_VERSION=2
            -sMIN_WEBGL_VERSION=2
            # PTHREAD_POOL_SIZE must cover the worst-case concurrent job count
            # (kMaxColumnLoadsPerFrame=2 + kMaxMeshDispatchesPerFrame=8, plus
            # slack): a main thread that outruns the pool would have to block
            # to spawn a worker, and blocking the browser's main thread
            # deadlocks. Harmless when threads are off (the flag is ignored).
            -sPTHREAD_POOL_SIZE=16
            # PROXY_TO_PTHREAD moves main()/the game loop off the browser
            # main thread onto a worker -- required to fix the SDL pointer-
            # event mutex deadlock (item 37 follow-up). Once main() runs on
            # a worker, GL calls from it need OFFSCREEN_FRAMEBUFFER to reach
            # the real canvas, which the main thread still fully owns --
            # this proxies GL commands across the thread boundary (a real
            # but small per-call cost) without transferring the canvas.
            # Deliberately NOT -sOFFSCREENCANVAS_SUPPORT: that variant
            # hands the canvas itself to the worker via
            # transferControlToOffscreen(), which (a) aborted during
            # worker init in Firefox, (b) threw a DOMException in Chrome
            # the instant any JS on the main thread touched canvas.width/
            # height (shell.html's own resize handler did exactly that),
            # and (c) broke SDL's mouse/pointer-lock handling on the
            # now-placeholder main-thread canvas -- a real user-reported
            # regression from an earlier version of this build. Don't
            # re-add it without solving all three.
            $<$<BOOL:${CNA_CRAFT_WASM_THREADS}>:-sPROXY_TO_PTHREAD>
            $<$<BOOL:${CNA_CRAFT_WASM_THREADS}>:-sOFFSCREEN_FRAMEBUFFER=1>
            "SHELL:--pre-js ${CMAKE_CURRENT_SOURCE_DIR}/web/pre.js"
            "SHELL:--shell-file ${CMAKE_CURRENT_SOURCE_DIR}/web/shell.html"
        )
        set_property(TARGET CnaCraft APPEND PROPERTY LINK_DEPENDS
                     "${CMAKE_CURRENT_SOURCE_DIR}/web/pre.js"
                     "${CMAKE_CURRENT_SOURCE_DIR}/web/shell.html")
    else()
        if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang" AND NOT WIN32 AND _cna_craft_backend_target)
            target_link_libraries(CnaCraft PRIVATE
                -Wl,--start-group CNA ${_cna_craft_backend_target} -Wl,--end-group
                SHARP_RUNTIME CnaCraftWorlds CnaCraftNetTransport)
        else()
            target_link_libraries(CnaCraft PRIVATE CNA SHARP_RUNTIME CnaCraftWorlds CnaCraftNetTransport)
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
endif()
