cmake_minimum_required(VERSION 3.21)

project(CnaTamagotchi LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# CNA is intentionally a sibling checkout, not a vendored submodule.
# Override this cache value when the framework lives elsewhere.
set(CNA_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../cna" CACHE PATH
    "Path to the CNA framework checkout")

if(NOT EXISTS "${CNA_ROOT_DIR}/CMakeLists.txt")
    message(FATAL_ERROR
        "cna-tamagotchi: CNA was not found at '${CNA_ROOT_DIR}'.\n"
        "Clone https://github.com/openeggbert/cna next to this project, or set\n"
        "-DCNA_ROOT_DIR=/path/to/cna.")
endif()

# The game is 2D. SDL_RENDERER is the most portable default and is also
# appropriate for early application work. CNA may offer additional
# backends; callers can select one explicitly from this supported set.
set(CNA_TAMAGOTCHI_BACKENDS SDL_RENDERER EASYGL HEADLESS SOFTWARE)
if(ANDROID OR EMSCRIPTEN)
    set(CNA_GRAPHICS_BACKEND "SDL_RENDERER" CACHE STRING
        "CNA graphics backend" FORCE)
else()
    set(CNA_GRAPHICS_BACKEND "SDL_RENDERER" CACHE STRING
        "CNA graphics backend")
endif()
set_property(CACHE CNA_GRAPHICS_BACKEND PROPERTY STRINGS ${CNA_TAMAGOTCHI_BACKENDS})

if(NOT CNA_GRAPHICS_BACKEND IN_LIST CNA_TAMAGOTCHI_BACKENDS)
    message(FATAL_ERROR
        "cna-tamagotchi: unsupported CNA_GRAPHICS_BACKEND='${CNA_GRAPHICS_BACKEND}'. "
        "Choose one of: ${CNA_TAMAGOTCHI_BACKENDS}")
endif()

# The framework's own samples/tests are not part of this game's build.
set(CNA_BUILD_TESTS OFF CACHE BOOL "Build CNA tests" FORCE)
set(CNA_BUILD_EXAMPLES OFF CACHE BOOL "Build CNA examples" FORCE)
# CNA enables ccache when it happens to be installed. Keep this project's
# default self-contained: a caller who has a writable shared ccache can still
# opt in explicitly with -DCNA_USE_CCACHE=ON.
set(CNA_USE_CCACHE OFF CACHE BOOL "Use ccache for CNA and sharp-runtime builds")
add_subdirectory("${CNA_ROOT_DIR}" cna)

set(CNA_TAMAGOTCHI_SOURCES
    src/main.cpp
    src/Application/CnaTamagotchiGame.cpp
    src/Display/MonochromeDisplay.cpp
)

# Android's SDL activity loads libmain.so; desktop builds a normal executable.
if(ANDROID)
    set(CNA_TAMAGOTCHI_TARGET main)
    add_library(${CNA_TAMAGOTCHI_TARGET} SHARED ${CNA_TAMAGOTCHI_SOURCES})
else()
    set(CNA_TAMAGOTCHI_TARGET CnaTamagotchi)
    add_executable(${CNA_TAMAGOTCHI_TARGET} ${CNA_TAMAGOTCHI_SOURCES})
endif()

if(EMSCRIPTEN)
    set_target_properties(${CNA_TAMAGOTCHI_TARGET} PROPERTIES SUFFIX ".html")
endif()

target_include_directories(${CNA_TAMAGOTCHI_TARGET} PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include")

# Imported SDL targets are directory-scoped, so discover SDL3 again after
# CNA configures its dependencies.
find_package(SDL3 REQUIRED CONFIG)

string(TOLOWER "${CNA_GRAPHICS_BACKEND}" _cna_backend_lower)
set(_cna_backend_target "cna_backend_graphics_${_cna_backend_lower}")
if(NOT TARGET ${_cna_backend_target})
    message(FATAL_ERROR
        "cna-tamagotchi: CNA backend target '${_cna_backend_target}' was not created.")
endif()

# GNU ld needs a link group because CNA and its selected backend have
# circular static-library references.
if(EMSCRIPTEN)
    target_link_libraries(${CNA_TAMAGOTCHI_TARGET} PRIVATE CNA SDL3::SDL3-static)
elseif(ANDROID)
    target_link_libraries(${CNA_TAMAGOTCHI_TARGET} PRIVATE CNA SDL3::SDL3 android log)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang" AND NOT WIN32)
    target_link_libraries(${CNA_TAMAGOTCHI_TARGET} PRIVATE
        -Wl,--start-group CNA ${_cna_backend_target} -Wl,--end-group
        SDL3::SDL3)
else()
    target_link_libraries(${CNA_TAMAGOTCHI_TARGET} PRIVATE CNA SDL3::SDL3)
endif()

if(TARGET SDL3::SDL3main)
    target_link_libraries(${CNA_TAMAGOTCHI_TARGET} PRIVATE SDL3::SDL3main)
endif()

option(CNA_TAMAGOTCHI_BUILD_TESTS "Build framework-free domain tests" ON)
if(CNA_TAMAGOTCHI_BUILD_TESTS AND NOT ANDROID AND NOT EMSCRIPTEN AND NOT CMAKE_CROSSCOMPILING)
    enable_testing()
    add_executable(CnaTamagotchiDomainTests
        tests/PetSimulationTests.cpp
        src/Domain/PetSimulation.cpp
    )
    target_include_directories(CnaTamagotchiDomainTests PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/include")
    add_test(NAME CnaTamagotchiDomainTests COMMAND CnaTamagotchiDomainTests)
endif()
