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

add_subdirectory("${CNA_ROOT_DIR}" CNA)

# MAR-CNA-004: pinned CNA's vendored Draco 1.5.7 uses std::all_of in
# ply_reader.cc without including <algorithm>. Emscripten 6.0.3 no longer
# supplies that declaration transitively. Keep the workaround target-local and
# remove it once CNA's pinned source gains the missing include.
if(EMSCRIPTEN AND TARGET draco_io)
    target_compile_options(draco_io PRIVATE -include algorithm)
endif()

add_library(copper_boots_gameplay STATIC
    gameplay/src/Camera2D.cpp
    gameplay/src/GameSettings.cpp
    gameplay/src/InputActionAdapter.cpp
    gameplay/src/LevelDefinition.cpp
    gameplay/src/ProceduralAudio.cpp
    gameplay/src/PresentationLayout.cpp
    gameplay/src/ProgressSave.cpp
    gameplay/src/SimulationClock.cpp
    gameplay/src/TileMap.cpp
    gameplay/src/WorldSimulation.cpp
)
target_include_directories(copper_boots_gameplay PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/gameplay/include")
target_compile_features(copper_boots_gameplay PUBLIC cxx_std_23)

add_executable(copper-boots
    game/src/CnaProgressStore.cpp
    game/src/CnaSettingsStore.cpp
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

if(EMSCRIPTEN)
    # Emit a directly serveable browser page and preload the project-created
    # content into Emscripten's virtual filesystem for CNA TitleContainer.
    set_target_properties(copper-boots PROPERTIES SUFFIX ".html")
    # MAR-CNA-006: CNA's Emscripten exception ABI options are directory-local
    # when CNA is embedded, so apply the same ABI to consumer-owned objects and
    # the final link until CNA publishes them through its public target.
    target_compile_options(copper_boots_gameplay PRIVATE
        -fwasm-exceptions -sWASM_LEGACY_EXCEPTIONS=0)
    target_compile_options(copper-boots PRIVATE
        -fwasm-exceptions -sWASM_LEGACY_EXCEPTIONS=0)
    target_link_options(copper-boots PRIVATE
        -fwasm-exceptions -sWASM_LEGACY_EXCEPTIONS=0
        -sALLOW_MEMORY_GROWTH=1
        "SHELL:--preload-file ${CMAKE_CURRENT_SOURCE_DIR}/Content@/Content")
endif()

add_custom_target(copper_boots_content
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/Content"
        "$<TARGET_FILE_DIR:copper-boots>/Content"
    COMMENT "Synchronizing Copper Boots content")
add_dependencies(copper-boots copper_boots_content)

if(MSVC)
    target_compile_options(copper_boots_gameplay PRIVATE /W4 /WX)
    target_compile_options(copper-boots PRIVATE /W4 /WX)
else()
    target_compile_options(copper_boots_gameplay PRIVATE
        -Wall -Wextra -Werror)
    target_compile_options(copper-boots PRIVATE
        -Wall -Wextra -Werror)
    # MAR-CNA-005: Clang 22 diagnoses three issues in transitive pinned-CNA
    # public headers. Preserve -Werror for Copper Boots while leaving these
    # dependency-owned diagnostics visible as warnings in the web build.
    if(EMSCRIPTEN)
        target_compile_options(copper-boots PRIVATE
            -Wno-error=defaulted-function-deleted
            -Wno-error=inconsistent-missing-override
            -Wno-error=overloaded-virtual)
    endif()
endif()

if(COPPER_BOOTS_BUILD_TESTS)
    enable_testing()
    add_executable(copper_boots_gameplay_tests
        test/GameplayTests.cpp)
    target_link_libraries(copper_boots_gameplay_tests PRIVATE
        copper_boots_gameplay)
    add_dependencies(copper_boots_gameplay_tests copper_boots_content)
    if(MSVC)
        target_compile_options(copper_boots_gameplay_tests PRIVATE /W4 /WX)
    else()
        target_compile_options(copper_boots_gameplay_tests PRIVATE
            -Wall -Wextra -Werror)
    endif()
    add_test(NAME copper_boots_gameplay COMMAND copper_boots_gameplay_tests)
    set_tests_properties(copper_boots_gameplay PROPERTIES
        LABELS "logic;deterministic")

    if(NOT CMAKE_CROSSCOMPILING AND NOT EMSCRIPTEN)
        add_test(NAME copper_boots_smoke
            COMMAND copper-boots --smoke-test --no-settings)
        add_test(NAME copper_boots_smoke_silent
            COMMAND copper-boots --smoke-test --no-audio --no-settings)
        add_test(NAME copper_boots_factory_smoke
            COMMAND copper-boots --smoke-test --no-audio --no-settings
                --stage 2)
        set_tests_properties(copper_boots_smoke PROPERTIES
            TIMEOUT 20
            LABELS "graphics;smoke;cna;audio")
        set_tests_properties(copper_boots_smoke_silent PROPERTIES
            TIMEOUT 20
            LABELS "graphics;smoke;cna;audio-fallback")
        set_tests_properties(copper_boots_factory_smoke PROPERTIES
            TIMEOUT 20
            LABELS "graphics;smoke;cna;campaign;factory")
        add_test(NAME copper_boots_storage
            COMMAND copper-boots --storage-smoke-test)
        set_tests_properties(copper_boots_storage PROPERTIES
            TIMEOUT 20
            LABELS "storage;cna"
            ENVIRONMENT
                "XDG_DATA_HOME=${CMAKE_CURRENT_BINARY_DIR}/test-user-data")
        add_test(NAME copper_boots_display
            COMMAND copper-boots --display-smoke-test --no-audio --no-settings)
        set_tests_properties(copper_boots_display PROPERTIES
            TIMEOUT 20
            LABELS "graphics;smoke;cna;display;resize;fullscreen")
        if(UNIX AND NOT APPLE)
            set_tests_properties(
                copper_boots_smoke copper_boots_smoke_silent
                copper_boots_factory_smoke
                copper_boots_display PROPERTIES
                ENVIRONMENT "SDL_VIDEODRIVER=offscreen;SDL_AUDIODRIVER=dummy")
        endif()
    endif()
endif()
