cmake_minimum_required(VERSION 3.24)

project(IronGang
    VERSION 0.1.0
    DESCRIPTION "Original open-city action-adventure prototype built on CNA and sharp-runtime"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(IRON_GANG_BUILD_TESTS "Build Iron Gang core tests" ON)
option(IRON_GANG_WARNINGS_AS_ERRORS "Treat Iron Gang warnings as errors" OFF)
option(IRON_GANG_ENABLE_SANITIZERS "Enable AddressSanitizer and UndefinedBehaviorSanitizer" OFF)

set(IRON_GANG_CNA_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../cnanext" CACHE PATH
    "Path to the modular CNA source tree.")

if(NOT EXISTS "${IRON_GANG_CNA_DIR}/CMakeLists.txt")
    message(FATAL_ERROR
        "Iron Gang: modular CNA was not found at '${IRON_GANG_CNA_DIR}'.\n"
        "Recommended workspace layout:\n"
        "  workspace/cnanext\n"
        "  workspace/sharp-runtime\n"
        "  workspace/easy-gl       # for OPENGLES3\n"
        "  workspace/iron-gang\n"
        "Override with -DIRON_GANG_CNA_DIR=/absolute/path/to/cnanext")
endif()

get_filename_component(IRON_GANG_DEPENDENCY_ROOT "${IRON_GANG_CNA_DIR}/.." ABSOLUTE)
set(IRON_GANG_SHARP_RUNTIME_DIR "${IRON_GANG_DEPENDENCY_ROOT}/sharp-runtimenext" CACHE PATH
    "Path to the sharp-runtime checkout consumed by CNA")
if(NOT EXISTS "${IRON_GANG_SHARP_RUNTIME_DIR}/CMakeLists.txt")
    set(_iron_gang_legacy_sharp_runtime "${IRON_GANG_DEPENDENCY_ROOT}/sharp-runtime")
    if(EXISTS "${_iron_gang_legacy_sharp_runtime}/CMakeLists.txt")
        set(IRON_GANG_SHARP_RUNTIME_DIR "${_iron_gang_legacy_sharp_runtime}" CACHE PATH
            "Path to the sharp-runtime checkout consumed by CNA" FORCE)
    endif()
endif()
if(NOT EXISTS "${IRON_GANG_SHARP_RUNTIME_DIR}/CMakeLists.txt")
    message(FATAL_ERROR
        "Iron Gang: sharp-runtime was not found at '${IRON_GANG_SHARP_RUNTIME_DIR}'.\n"
        "Override with -DIRON_GANG_SHARP_RUNTIME_DIR=/absolute/path/to/sharp-runtimenext")
endif()

# Jolt Physics (selected in plan_15-physics-integration.md IG-15-003): a shared checkout under
# ~/deps/jolt per CLAUDE.md's dependency-reuse convention, not a per-project clone.
set(IRON_GANG_JOLT_DIR "$ENV{HOME}/deps/jolt" CACHE PATH
    "Path to a Jolt Physics checkout (pinned to v5.6.0; see ~/deps/<name> convention in CLAUDE.md).")

if(NOT EXISTS "${IRON_GANG_JOLT_DIR}/Build/CMakeLists.txt")
    message(FATAL_ERROR
        "Iron Gang: Jolt Physics was not found at '${IRON_GANG_JOLT_DIR}'.\n"
        "Clone it once into the shared deps cache:\n"
        "  git clone --branch v5.6.0 --depth 1 https://github.com/jrouwe/JoltPhysics.git ~/deps/jolt\n"
        "Override with -DIRON_GANG_JOLT_DIR=/absolute/path/to/JoltPhysics")
endif()

# Consumer-side choices. The project does not build dependency tests/examples by default.
set(CNA_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(CNA_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(CNA_ENABLE_NET OFF CACHE BOOL "" FORCE)
set(CNA_DEVICES OFF CACHE BOOL "" FORCE)
set(CNA_CNAEXT OFF CACHE BOOL "" FORCE)
set(CNA_ENABLE_DRACO OFF CACHE BOOL "" FORCE)
set(SHARP_RUNTIME_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(SHARP_RUNTIME_BUILD_BENCHMARKS OFF CACHE BOOL "" FORCE)
# CNA's modules name their own narrow Sharp Runtime dependencies. Its renderer target currently
# consumes CNA's documented framework closure; Iron Gang adds only Text.Json for mission and
# cutscene data. Setting the selection before adding CNA prevents sharp-runtime's `All` umbrella
# from instantiating unrelated networking, diagnostics, XML, compression, and process modules.
set(SHARP_RUNTIME_COMPONENTS
    "Core.Base;IO;Collections.Core;Collections.ObjectModel;Runtime;Threading;Text;Text.Json;Globalization;Storage;Security.Cryptography"
    CACHE STRING "Sharp Runtime components required by modular CNA and Iron Gang" FORCE)
set(CNA_SHARP_RUNTIME_ROOT "${IRON_GANG_SHARP_RUNTIME_DIR}" CACHE PATH
    "Path to the sharp-runtime checkout consumed by CNA" FORCE)
# Jolt defaults INTERPROCEDURAL_OPTIMIZATION ON and expects the *consuming* project to also opt
# in via CMAKE_INTERPROCEDURAL_OPTIMIZATION; since Iron Gang does not, disable it for Jolt
# rather than risk an LTO link mismatch (Jolt's own CMakeLists.txt documents this exact pitfall).
set(INTERPROCEDURAL_OPTIMIZATION OFF CACHE BOOL "" FORCE)
# GPU-compute shader paths (optional hair/cloth demos) are irrelevant to Iron Gang's CPU rigid-
# body/character/vehicle use and require dxc/glslc toolchains this workspace does not have.
set(JPH_USE_DX12 OFF CACHE BOOL "" FORCE)
set(JPH_USE_VK OFF CACHE BOOL "" FORCE)
set(JPH_USE_MTL OFF CACHE BOOL "" FORCE)
set(JPH_USE_CPU_COMPUTE OFF CACHE BOOL "" FORCE)

# OPENGLES3 uses CNA's EasyGL implementation and remains the recommended prototype renderer.
# Override with VULKAN where available.
if(NOT DEFINED CNA_GRAPHICS_RENDERER)
    set(CNA_GRAPHICS_RENDERER "OPENGLES3" CACHE STRING "CNA graphics renderer")
endif()

# Keep CNA's module targets available, but do not make every module/tool part of Iron Gang's
# default `all` target. Only the module closure reached from CNA::GraphicsCore/CNA::Runtime below
# is built; optional Devices, GraphicsExt, C API, tools, and descriptor gates remain opt-in.
add_subdirectory("${IRON_GANG_CNA_DIR}" "${CMAKE_BINARY_DIR}/_deps/cna" EXCLUDE_FROM_ALL)

add_subdirectory("${IRON_GANG_JOLT_DIR}/Build" "${CMAKE_BINARY_DIR}/_deps/jolt")

include(cmake/CompilerWarnings.cmake)

add_library(iron_gang_core STATIC
    src/Core/PerformanceProfiler.cpp
    src/Cutscenes/CutscenePlayer.cpp
    src/Cutscenes/CutsceneSequence.cpp
    src/Dialogue/DialogueSystem.cpp
    src/Gameplay/Pedestrian.cpp
    src/Gameplay/PlayerController.cpp
    src/Gameplay/PoliceSystem.cpp
    src/Gameplay/TrafficVehicle.cpp
    src/Gameplay/VehicleController.cpp
    src/Graphics/LightmapMesh.cpp
    src/Graphics/VideoMemoryAccounting.cpp
    src/Missions/MissionDefinition.cpp
    src/Missions/PrototypeMission.cpp
    src/Persistence/SaveGame.cpp
    src/Physics/PhysicsWorld.cpp
    src/UI/BitmapFont.cpp
    src/UI/DistrictMap.cpp
    src/World/DistrictManager.cpp
    src/World/PrototypeWorld.cpp
    src/World/WaypointPath.cpp)

add_library(IronGang::Core ALIAS iron_gang_core)

target_include_directories(iron_gang_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(iron_gang_core
    PUBLIC
        CNA::GraphicsCore
    PRIVATE
        SharpRuntime::IO
        SharpRuntime::Text.Json
        Jolt)

# Jolt's own headers are not warning-clean under this project's strict flags; treat them as
# system headers (matching sharp-runtime/vendor's existing -isystem treatment) so only Iron
# Gang's own code in PhysicsWorld.cpp is held to iron_gang_apply_warnings() below.
get_target_property(IRON_GANG_JOLT_INCLUDE_DIRS Jolt INTERFACE_INCLUDE_DIRECTORIES)
if(IRON_GANG_JOLT_INCLUDE_DIRS)
    target_include_directories(iron_gang_core SYSTEM PRIVATE ${IRON_GANG_JOLT_INCLUDE_DIRS})
endif()

iron_gang_apply_warnings(iron_gang_core)

add_executable(iron_gang
    src/Application/IronGangGame.cpp
    src/Graphics/GpuFrameTimer.cpp
    src/Graphics/PrimitiveMesh.cpp
    src/Graphics/PrototypeRenderer.cpp
    src/main.cpp)

add_executable(IronGang::Game ALIAS iron_gang)

target_include_directories(iron_gang
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_compile_definitions(iron_gang PRIVATE
    IRON_GANG_VERSION="${PROJECT_VERSION}"
    IRON_GANG_DEFAULT_ASSET_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets"
    IRON_GANG_GRAPHICS_BACKEND="${CNA_GRAPHICS_RENDERER}"
    IRON_GANG_BUILD_CONFIGURATION="${CMAKE_BUILD_TYPE}")

target_link_libraries(iron_gang PRIVATE
    iron_gang_core
    CNA::Runtime)
# GpuFrameTimer uses CNA's renderer timer contract, whose broad interface supplies many default
# no-op methods with intentionally unnamed-at-call-site parameters. Game.hpp also reaches
# sharp-runtime's Decimal.hpp through CNA's public content headers. Mark both dependency roots as
# external for Iron Gang so strict warnings diagnose our sources rather than dependency internals.
target_include_directories(iron_gang SYSTEM PRIVATE
    "${IRON_GANG_CNA_DIR}/modules/graphics/include"
    "${IRON_GANG_SHARP_RUNTIME_DIR}/modules/core/include")
iron_gang_apply_warnings(iron_gang)

if(TARGET SDL3::SDL3main)
    target_link_libraries(iron_gang PRIVATE SDL3::SDL3main)
endif()

if(EMSCRIPTEN)
    # CNA's browser path uses SDL3 + WebGL2. Keep the game's data beside the generated
    # launcher so the same relative assetRoot ("assets") works in the browser filesystem.
    target_link_options(iron_gang PRIVATE
        "--preload-file=${CMAKE_CURRENT_SOURCE_DIR}/assets@assets"
        "-sUSE_WEBGL2=1"
        "-sALLOW_MEMORY_GROWTH=1"
        "-sINITIAL_MEMORY=268435456"
        "-sNO_EXIT_RUNTIME=1")
    set_target_properties(iron_gang PROPERTIES
        SUFFIX ".js"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/web/build")
endif()

if(WIN32)
    set_target_properties(iron_gang PROPERTIES WIN32_EXECUTABLE TRUE)
    if(COMMAND cna_copy_sdl_runtime)
        cna_copy_sdl_runtime(iron_gang)
    endif()
endif()

set_target_properties(iron_gang PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")

if(IRON_GANG_ENABLE_SANITIZERS AND NOT MSVC)
    foreach(target iron_gang_core iron_gang)
        target_compile_options(${target} PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
        target_link_options(${target} PRIVATE -fsanitize=address,undefined)
    endforeach()
endif()

if(IRON_GANG_BUILD_TESTS)
    enable_testing()
    find_package(Python3 REQUIRED COMPONENTS Interpreter)

    add_test(NAME iron_gang_performance_report_tests
        COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_performance_report.py"
            "${CMAKE_CURRENT_SOURCE_DIR}/scripts/performance_report.py"
            "${CMAKE_CURRENT_SOURCE_DIR}/scripts/vram_evidence.py")
    add_test(NAME iron_gang_performance_compare_tests
        COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_performance_compare.py"
            "${CMAKE_CURRENT_SOURCE_DIR}/scripts/performance_compare.py")
    add_test(NAME iron_gang_vram_evidence_tests
        COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_vram_evidence.py"
            "${CMAKE_CURRENT_SOURCE_DIR}/scripts/vram_evidence.py"
            "${CMAKE_CURRENT_SOURCE_DIR}/scripts/performance_report.py"
            "${CMAKE_CURRENT_SOURCE_DIR}/scripts/performance_compare.py"
            "${CMAKE_CURRENT_SOURCE_DIR}/scripts/drm_vram_capture.py")
    add_test(NAME iron_gang_content_budget_tests
        COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_content_budget.py"
            "${CMAKE_CURRENT_SOURCE_DIR}/scripts/content_budget.py"
            "${CMAKE_CURRENT_SOURCE_DIR}")

    add_executable(iron_gang_core_tests tests/CoreTests.cpp)
    target_include_directories(iron_gang_core_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
    # Gate M7: lets TestMissionLoadsCommittedFile() validate the real, committed
    # assets/missions/prologue.mission.json (not just a hand-written test fixture), matching
    # iron_gang's own IRON_GANG_DEFAULT_ASSET_DIR convention.
    target_compile_definitions(iron_gang_core_tests PRIVATE
        IRON_GANG_SOURCE_ASSET_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets")
    target_link_libraries(iron_gang_core_tests PRIVATE iron_gang_core)
    iron_gang_apply_warnings(iron_gang_core_tests)
    add_test(NAME iron_gang_core_tests COMMAND iron_gang_core_tests)
    set_tests_properties(iron_gang_core_tests PROPERTIES
        WORKING_DIRECTORY "${CMAKE_BINARY_DIR}")

    # IG-39-035: the game must fall back to the procedural warehouse box, not crash, when
    # scripts/build-assets.sh has not produced assets/generated/warehouse/cnj/warehouse.cnj.
    add_test(NAME iron_gang_missing_asset_fallback
        COMMAND bash "${CMAKE_CURRENT_SOURCE_DIR}/scripts/test-missing-asset-fallback.sh" "$<TARGET_FILE:iron_gang>")
    set_tests_properties(iron_gang_missing_asset_fallback PROPERTIES
        WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
        # A regression test must not depend on the host's PulseAudio/PipeWire permissions. CNA's
        # SDL3 audio path is still exercised, through SDL's deterministic no-hardware driver.
        ENVIRONMENT "SDL_AUDIODRIVER=dummy")

    # Gate M4 (IG-39-005): character, trigger, raycast, and vehicle prototypes behind PhysicsWorld.
    add_executable(iron_gang_physics_tests tests/PhysicsTests.cpp)
    target_include_directories(iron_gang_physics_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
    target_link_libraries(iron_gang_physics_tests PRIVATE iron_gang_core)
    iron_gang_apply_warnings(iron_gang_physics_tests)
    add_test(NAME iron_gang_physics_tests COMMAND iron_gang_physics_tests)
    set_tests_properties(iron_gang_physics_tests PROPERTIES
        WORKING_DIRECTORY "${CMAKE_BINARY_DIR}")

    # Gate M12 / IG-35-051: bounded no-window lifecycle soak for district, mission, and save/load.
    add_executable(iron_gang_memory_soak_tests tests/MemorySoakTests.cpp)
    target_include_directories(iron_gang_memory_soak_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
    target_link_libraries(iron_gang_memory_soak_tests PRIVATE iron_gang_core)
    iron_gang_apply_warnings(iron_gang_memory_soak_tests)
    add_test(NAME iron_gang_memory_soak_tests COMMAND iron_gang_memory_soak_tests --cycles 200)
    set_tests_properties(iron_gang_memory_soak_tests PROPERTIES
        WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
        LABELS "soak;performance"
        TIMEOUT 60)
endif()

install(TARGETS iron_gang RUNTIME DESTINATION bin)
install(DIRECTORY assets/ DESTINATION share/iron-gang/assets)
install(FILES README.md LICENSE THIRD_PARTY.md DESTINATION share/iron-gang)
