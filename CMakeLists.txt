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
set(CNA_TAMAGOTCHI_RENDERERS SDL_RENDERER HEADLESS)
if(ANDROID OR EMSCRIPTEN)
    set(CNA_GRAPHICS_RENDERER "SDL_RENDERER" CACHE STRING
        "CNA graphics renderer" FORCE)
else()
    set(CNA_GRAPHICS_RENDERER "SDL_RENDERER" CACHE STRING
        "CNA graphics renderer")
endif()
set_property(CACHE CNA_GRAPHICS_RENDERER PROPERTY STRINGS ${CNA_TAMAGOTCHI_RENDERERS})

if(NOT CNA_GRAPHICS_RENDERER IN_LIST CNA_TAMAGOTCHI_RENDERERS)
    message(FATAL_ERROR
        "tamagotchi-cna: unsupported CNA_GRAPHICS_RENDERER='${CNA_GRAPHICS_RENDERER}'. "
        "Choose one of: ${CNA_TAMAGOTCHI_RENDERERS}")
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

set(CNA_TAMAGOTCHI_SOURCES
    src/main.cpp
    src/Application/CnaTamagotchiGame.cpp
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
    set(CNA_TAMAGOTCHI_TARGET main)
    add_library(${CNA_TAMAGOTCHI_TARGET} SHARED ${CNA_TAMAGOTCHI_SOURCES})
else()
    set(CNA_TAMAGOTCHI_TARGET TamagotchiCna)
    add_executable(${CNA_TAMAGOTCHI_TARGET} ${CNA_TAMAGOTCHI_SOURCES})
endif()

if(EMSCRIPTEN)
    set_target_properties(${CNA_TAMAGOTCHI_TARGET} PROPERTIES SUFFIX ".html")
endif()

target_include_directories(${CNA_TAMAGOTCHI_TARGET} PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include")

configure_file(assets/p1-icon-atlas.png assets/p1-icon-atlas.png COPYONLY)
configure_file(assets/p1-icon-atlas-smooth.png assets/p1-icon-atlas-smooth.png COPYONLY)

# Link only the framework modules this application consumes.  CNA::Runtime
# closes over graphics, input, audio, media, content, math and platform; the
# selected renderer closes the final graphics cycle.  This deliberately avoids
# CNA's compatibility umbrella, which also brings in devices, networking,
# storage and CNAEXT.
if(CNA_GRAPHICS_RENDERER STREQUAL "SDL_RENDERER")
    set(CNA_TAMAGOTCHI_RENDERER_TARGET cna_renderer_sdl_renderer)
elseif(CNA_GRAPHICS_RENDERER STREQUAL "HEADLESS")
    set(CNA_TAMAGOTCHI_RENDERER_TARGET cna_renderer_headless)
endif()

if(NOT TARGET ${CNA_TAMAGOTCHI_RENDERER_TARGET})
    message(FATAL_ERROR
        "tamagotchi-cna: CNA renderer target '${CNA_TAMAGOTCHI_RENDERER_TARGET}' was not created.")
endif()

target_link_libraries(${CNA_TAMAGOTCHI_TARGET} PRIVATE
    CNA::Runtime
    ${CNA_TAMAGOTCHI_RENDERER_TARGET})

if(ANDROID)
    # CNA's mobile entry-point header directly includes SDL_main.h.
    find_package(SDL3 REQUIRED CONFIG)
    target_link_libraries(${CNA_TAMAGOTCHI_TARGET} PRIVATE SDL3::SDL3 android log)
    if(TARGET SDL3::SDL3main)
        target_link_libraries(${CNA_TAMAGOTCHI_TARGET} PRIVATE SDL3::SDL3main)
    endif()
endif()

option(CNA_TAMAGOTCHI_BUILD_TESTS "Build framework-free domain tests" ON)
if(CNA_TAMAGOTCHI_BUILD_TESTS AND NOT ANDROID AND NOT EMSCRIPTEN AND NOT CMAKE_CROSSCOMPILING)
    enable_testing()
    add_executable(CnaTamagotchiP1StateTests
        tests/P1StateTests.cpp
    )
    target_include_directories(CnaTamagotchiP1StateTests PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/include")
    add_test(NAME CnaTamagotchiP1StateTests COMMAND CnaTamagotchiP1StateTests)

    add_executable(CnaTamagotchiP1ProgramTests
        tests/P1ProgramTests.cpp
        src/Domain/P1Program.cpp
    )
    target_include_directories(CnaTamagotchiP1ProgramTests PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/include")
    add_test(NAME CnaTamagotchiP1ProgramTests COMMAND CnaTamagotchiP1ProgramTests)

    add_executable(CnaTamagotchiP1SpriteCatalogTests
        tests/P1SpriteCatalogTests.cpp
        src/Domain/P1SpriteCatalog.cpp
    )
    target_include_directories(CnaTamagotchiP1SpriteCatalogTests PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/include")
    add_test(NAME CnaTamagotchiP1SpriteCatalogTests COMMAND CnaTamagotchiP1SpriteCatalogTests)

    add_executable(CnaTamagotchiProgramSimulationTests
        tests/ProgramSimulationTests.cpp
        src/Domain/P1Program.cpp
        src/Domain/ProgramSimulation.cpp
    )
    target_include_directories(CnaTamagotchiProgramSimulationTests PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/include")
    add_test(NAME CnaTamagotchiProgramSimulationTests COMMAND CnaTamagotchiProgramSimulationTests)

    add_executable(CnaTamagotchiDisplayTests
        tests/MonochromeDisplayTests.cpp
        src/Display/MonochromeDisplay.cpp
        src/Display/P1LightScreen.cpp
        src/Display/P1MedicineAnimation.cpp
        src/Display/P1ToiletWipe.cpp
    )
    target_include_directories(CnaTamagotchiDisplayTests PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/include")
    add_test(NAME CnaTamagotchiDisplayTests COMMAND CnaTamagotchiDisplayTests)

    add_executable(CnaTamagotchiSaveLocationTests
        tests/SaveLocationTests.cpp
        src/Persistence/SaveLocation.cpp
    )
    target_include_directories(CnaTamagotchiSaveLocationTests PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/include")
    add_test(NAME CnaTamagotchiSaveLocationTests COMMAND CnaTamagotchiSaveLocationTests)

    add_executable(CnaTamagotchiPersistenceTests
        tests/SaveRepositoryTests.cpp
        src/Persistence/SaveRepository.cpp
    )
    target_include_directories(CnaTamagotchiPersistenceTests PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/include")
    add_test(NAME CnaTamagotchiPersistenceTests COMMAND CnaTamagotchiPersistenceTests)

    add_executable(CnaTamagotchiDeviceShellTests
        tests/DeviceShellTests.cpp
    )
    target_include_directories(CnaTamagotchiDeviceShellTests PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/include")
    add_test(NAME CnaTamagotchiDeviceShellTests COMMAND CnaTamagotchiDeviceShellTests)
endif()
