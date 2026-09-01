cmake_minimum_required(VERSION 3.21)

project(TamagotchiCna LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# CNA is intentionally a sibling checkout, not a vendored submodule.
# Override this cache value when the framework lives elsewhere.
set(CNA_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../cna" CACHE PATH
    "Path to the CNA framework checkout")

if(NOT EXISTS "${CNA_ROOT_DIR}/CMakeLists.txt")
    message(FATAL_ERROR
        "tamagotchi-cna: CNA was not found at '${CNA_ROOT_DIR}'.\n"
        "Clone https://github.com/openeggbert/cna next to this project, or set\n"
        "-DCNA_ROOT_DIR=/path/to/cna.")
endif()

# The game is 2D.  These are CNA's current renderer identities, rather than
# the retired CNA_GRAPHICS_BACKEND names.  Keep the selectable surface small:
# this game needs a real SDL window for play and a headless renderer for
# framework integration checks.
set(TAMAGOTCHI_CNA_RENDERERS SDL_RENDERER HEADLESS)
if(ANDROID OR EMSCRIPTEN)
    set(CNA_GRAPHICS_RENDERER "SDL_RENDERER" CACHE STRING
        "CNA graphics renderer" FORCE)
else()
    set(CNA_GRAPHICS_RENDERER "SDL_RENDERER" CACHE STRING
        "CNA graphics renderer")
endif()
set_property(CACHE CNA_GRAPHICS_RENDERER PROPERTY STRINGS ${TAMAGOTCHI_CNA_RENDERERS})

if(NOT CNA_GRAPHICS_RENDERER IN_LIST TAMAGOTCHI_CNA_RENDERERS)
    message(FATAL_ERROR
        "tamagotchi-cna: unsupported CNA_GRAPHICS_RENDERER='${CNA_GRAPHICS_RENDERER}'. "
        "Choose one of: ${TAMAGOTCHI_CNA_RENDERERS}")
endif()

# The framework's own samples/tests are not part of this game's build.
set(CNA_BUILD_TESTS OFF CACHE BOOL "Build CNA tests" FORCE)
set(CNA_BUILD_EXAMPLES OFF CACHE BOOL "Build CNA examples" FORCE)
# The P1 desktop device has no network, CNAEXT, or device/sensor integration.
# Set all three explicitly before CNA is added so a framework default cannot
# pull ENet or optional extension targets into this application's closure.
set(CNA_ENABLE_NET OFF CACHE BOOL "Build CNA networking" FORCE)
set(CNA_CNAEXT OFF CACHE BOOL "Enable CNA extended graphics layer" FORCE)
set(CNA_DEVICES OFF CACHE BOOL "Enable CNA device and sensor extensions" FORCE)
# The game has no glTF content path, so do not build CNA's optional Draco
# decoder and its encoder-side dependency graph.
set(CNA_ENABLE_DRACO OFF CACHE BOOL "Build CNA glTF Draco decoder" FORCE)
# CNA enables ccache when it happens to be installed. Keep this project's
# default self-contained: a caller who has a writable shared ccache can still
# opt in explicitly with -DCNA_USE_CCACHE=ON.
set(CNA_USE_CCACHE OFF CACHE BOOL "Use ccache for CNA and sharp-runtime builds")
# CNA::Runtime and the selected SDL renderer currently need this closure.  Set
# it before CNA adds sharp-runtime so the modular runtime never falls back to
# its unrelated `All` component set.
set(SHARP_RUNTIME_COMPONENTS
    "Core.Base;IO;Collections.Core;Collections.ObjectModel;Runtime;Threading;Text;Globalization;Storage;Security.Cryptography"
    CACHE STRING "Sharp Runtime components required by tamagotchi-cna and CNA")
add_subdirectory("${CNA_ROOT_DIR}" cna)

set(TAMAGOTCHI_CNA_SOURCES
    src/main.cpp
    src/Application/TamagotchiCnaGame.cpp
    src/Application/DeviceShellRenderer.cpp
    src/Display/MonochromeDisplay.cpp
    src/Display/P1LightScreen.cpp
    src/Display/P1MedicineAnimation.cpp
    src/Display/P1ToiletWipe.cpp
    src/Domain/P1Program.cpp
    src/Domain/P1SpriteCatalog.cpp
    src/Domain/ProgramSimulation.cpp
    src/Persistence/SaveLocation.cpp
    src/Persistence/SaveRepository.cpp
)

# Android's SDL activity loads libmain.so; desktop builds a normal executable.
if(ANDROID)
    set(TAMAGOTCHI_CNA_TARGET main)
    add_library(${TAMAGOTCHI_CNA_TARGET} SHARED ${TAMAGOTCHI_CNA_SOURCES})
else()
    set(TAMAGOTCHI_CNA_TARGET TamagotchiCna)
    add_executable(${TAMAGOTCHI_CNA_TARGET} ${TAMAGOTCHI_CNA_SOURCES})
endif()

if(EMSCRIPTEN)
    set_target_properties(${TAMAGOTCHI_CNA_TARGET} PROPERTIES SUFFIX ".html")
    # CNA uses WebAssembly's native exception ABI.  Its directory-scoped flags
    # do not propagate back to this parent target, so keep the final game link
    # on the same ABI and package the runtime-loaded icon atlas into the web FS.
    target_compile_options(${TAMAGOTCHI_CNA_TARGET} PRIVATE
        -fwasm-exceptions
        -sWASM_LEGACY_EXCEPTIONS=0)
    target_link_options(${TAMAGOTCHI_CNA_TARGET} PRIVATE
        -fwasm-exceptions
        -sWASM_LEGACY_EXCEPTIONS=0
        --preload-file
        "${CMAKE_CURRENT_BINARY_DIR}/assets@/assets")
endif()

target_include_directories(${TAMAGOTCHI_CNA_TARGET} PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include")

configure_file(assets/p1-icon-atlas.png assets/p1-icon-atlas.png COPYONLY)
configure_file(assets/p1-icon-atlas-smooth.png assets/p1-icon-atlas-smooth.png COPYONLY)

# Link only the framework modules this application consumes.  CNA::Runtime
# closes over graphics, input, audio, media, content, math and platform; the
# selected renderer closes the final graphics cycle.  This deliberately avoids
# CNA's compatibility umbrella, which also brings in devices, networking,
# storage and CNAEXT.
if(CNA_GRAPHICS_RENDERER STREQUAL "SDL_RENDERER")
    set(TAMAGOTCHI_CNA_RENDERER_TARGET cna_renderer_sdl_renderer)
elseif(CNA_GRAPHICS_RENDERER STREQUAL "HEADLESS")
    set(TAMAGOTCHI_CNA_RENDERER_TARGET cna_renderer_headless)
endif()

if(NOT TARGET ${TAMAGOTCHI_CNA_RENDERER_TARGET})
    message(FATAL_ERROR
        "tamagotchi-cna: CNA renderer target '${TAMAGOTCHI_CNA_RENDERER_TARGET}' was not created.")
endif()

target_link_libraries(${TAMAGOTCHI_CNA_TARGET} PRIVATE
    CNA::Runtime
    ${TAMAGOTCHI_CNA_RENDERER_TARGET})

if(ANDROID)
    # CNA's mobile entry-point header directly includes SDL_main.h.
    find_package(SDL3 REQUIRED CONFIG)
    target_link_libraries(${TAMAGOTCHI_CNA_TARGET} PRIVATE SDL3::SDL3 android log)
    if(TARGET SDL3::SDL3main)
        target_link_libraries(${TAMAGOTCHI_CNA_TARGET} PRIVATE SDL3::SDL3main)
    endif()
endif()

option(TAMAGOTCHI_CNA_BUILD_TESTS "Build framework-free domain tests" ON)
if(TAMAGOTCHI_CNA_BUILD_TESTS AND NOT ANDROID AND NOT EMSCRIPTEN AND NOT CMAKE_CROSSCOMPILING)
    enable_testing()
    add_executable(TamagotchiCnaP1StateTests
        tests/P1StateTests.cpp
    )
    target_include_directories(TamagotchiCnaP1StateTests PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/include")
    add_test(NAME TamagotchiCnaP1StateTests COMMAND TamagotchiCnaP1StateTests)

    add_executable(TamagotchiCnaP1ProgramTests
        tests/P1ProgramTests.cpp
        src/Domain/P1Program.cpp
    )
    target_include_directories(TamagotchiCnaP1ProgramTests PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/include")
    add_test(NAME TamagotchiCnaP1ProgramTests COMMAND TamagotchiCnaP1ProgramTests)

    add_executable(TamagotchiCnaP1SpriteCatalogTests
        tests/P1SpriteCatalogTests.cpp
        src/Domain/P1SpriteCatalog.cpp
    )
    target_include_directories(TamagotchiCnaP1SpriteCatalogTests PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/include")
    add_test(NAME TamagotchiCnaP1SpriteCatalogTests COMMAND TamagotchiCnaP1SpriteCatalogTests)

    add_executable(TamagotchiCnaProgramSimulationTests
        tests/ProgramSimulationTests.cpp
        src/Domain/P1Program.cpp
        src/Domain/ProgramSimulation.cpp
    )
    target_include_directories(TamagotchiCnaProgramSimulationTests PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/include")
    add_test(NAME TamagotchiCnaProgramSimulationTests COMMAND TamagotchiCnaProgramSimulationTests)

    add_executable(TamagotchiCnaDisplayTests
        tests/MonochromeDisplayTests.cpp
        src/Display/MonochromeDisplay.cpp
        src/Display/P1LightScreen.cpp
        src/Display/P1MedicineAnimation.cpp
        src/Display/P1ToiletWipe.cpp
    )
    target_include_directories(TamagotchiCnaDisplayTests PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/include")
    add_test(NAME TamagotchiCnaDisplayTests COMMAND TamagotchiCnaDisplayTests)

    add_executable(TamagotchiCnaSaveLocationTests
        tests/SaveLocationTests.cpp
        src/Persistence/SaveLocation.cpp
    )
    target_include_directories(TamagotchiCnaSaveLocationTests PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/include")
    add_test(NAME TamagotchiCnaSaveLocationTests COMMAND TamagotchiCnaSaveLocationTests)

    add_executable(TamagotchiCnaPersistenceTests
        tests/SaveRepositoryTests.cpp
        src/Persistence/SaveRepository.cpp
    )
    target_include_directories(TamagotchiCnaPersistenceTests PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/include")
    add_test(NAME TamagotchiCnaPersistenceTests COMMAND TamagotchiCnaPersistenceTests)

    add_executable(TamagotchiCnaDeviceShellTests
        tests/DeviceShellTests.cpp
    )
    target_include_directories(TamagotchiCnaDeviceShellTests PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/include")
    add_test(NAME TamagotchiCnaDeviceShellTests COMMAND TamagotchiCnaDeviceShellTests)
endif()
