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

set(IRON_GANG_CNA_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../cna" CACHE PATH
    "Path to the CNA source tree. CNA expects sharp-runtime next to itself.")

if(NOT EXISTS "${IRON_GANG_CNA_DIR}/CMakeLists.txt")
    message(FATAL_ERROR
        "Iron Gang: CNA was not found at '${IRON_GANG_CNA_DIR}'.\n"
        "Recommended workspace layout:\n"
        "  workspace/cna\n"
        "  workspace/sharp-runtime\n"
        "  workspace/easy-gl       # for EASYGL\n"
        "  workspace/cna-extended\n"
        "  workspace/iron-gang\n"
        "Override with -DIRON_GANG_CNA_DIR=/absolute/path/to/cna")
endif()

get_filename_component(IRON_GANG_DEPENDENCY_ROOT "${IRON_GANG_CNA_DIR}/.." ABSOLUTE)
if(NOT EXISTS "${IRON_GANG_DEPENDENCY_ROOT}/sharp-runtime/CMakeLists.txt")
    message(FATAL_ERROR
        "Iron Gang: sharp-runtime must be a sibling of CNA at\n"
        "  ${IRON_GANG_DEPENDENCY_ROOT}/sharp-runtime\n"
        "This is required by CNA's current build contract.")
endif()

set(IRON_GANG_CNA_EXTENDED_DIR "${IRON_GANG_DEPENDENCY_ROOT}/cna-extended" CACHE PATH
    "Path to the cna-extended repository (ECS, 3D transform hierarchy, 3D collision/octree, skinned-model playback).")

if(NOT EXISTS "${IRON_GANG_CNA_EXTENDED_DIR}/CMakeLists.txt")
    message(FATAL_ERROR
        "Iron Gang: cna-extended must be a sibling of CNA at\n"
        "  ${IRON_GANG_CNA_EXTENDED_DIR}\n"
        "Override with -DIRON_GANG_CNA_EXTENDED_DIR=/absolute/path/to/cna-extended")
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
set(SHARP_RUNTIME_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(SHARP_RUNTIME_BUILD_BENCHMARKS OFF CACHE BOOL "" FORCE)
set(CNA_EXTENDED_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(CNA_EXTENDED_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
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

# EASYGL is the recommended prototype backend. Override with VULKAN where available.
if(NOT DEFINED CNA_GRAPHICS_BACKEND)
    set(CNA_GRAPHICS_BACKEND "EASYGL" CACHE STRING "CNA graphics backend")
endif()

add_subdirectory("${IRON_GANG_CNA_DIR}" "${CMAKE_BINARY_DIR}/_deps/cna")

# cna-extended detects the `CNA` target this project just defined above and
# links against it instead of building its own copy of CNA.
add_subdirectory("${IRON_GANG_CNA_EXTENDED_DIR}" "${CMAKE_BINARY_DIR}/_deps/cna_extended")

add_subdirectory("${IRON_GANG_JOLT_DIR}/Build" "${CMAKE_BINARY_DIR}/_deps/jolt")

include(cmake/CompilerWarnings.cmake)
include(cmake/CnaLinking.cmake)

add_library(iron_gang_core STATIC
    src/Cutscenes/CutscenePlayer.cpp
    src/Cutscenes/CutsceneSequence.cpp
    src/Dialogue/DialogueSystem.cpp
    src/Gameplay/Pedestrian.cpp
    src/Gameplay/PlayerController.cpp
    src/Gameplay/PoliceSystem.cpp
    src/Gameplay/TrafficVehicle.cpp
    src/Gameplay/VehicleController.cpp
    src/Graphics/LightmapMesh.cpp
    src/Missions/MissionDefinition.cpp
    src/Missions/PrototypeMission.cpp
    src/Persistence/SaveGame.cpp
    src/Physics/PhysicsWorld.cpp
    src/UI/BitmapFont.cpp
    src/World/DistrictManager.cpp
    src/World/PrototypeWorld.cpp
    src/World/WaypointPath.cpp)

add_library(IronGang::Core ALIAS iron_gang_core)

target_include_directories(iron_gang_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(iron_gang_core
    PUBLIC
        CNA
        SHARP_RUNTIME
        CNA_EXTENDED
    PRIVATE
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
    src/Graphics/PrimitiveMesh.cpp
    src/Graphics/PrototypeRenderer.cpp
    src/main.cpp)

add_executable(IronGang::Game ALIAS iron_gang)

target_include_directories(iron_gang
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_compile_definitions(iron_gang PRIVATE
    IRON_GANG_VERSION="${PROJECT_VERSION}"
    IRON_GANG_DEFAULT_ASSET_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets")

target_link_libraries(iron_gang PRIVATE iron_gang_core)
iron_gang_link_final_target(iron_gang)
iron_gang_apply_warnings(iron_gang)

if(TARGET SDL3::SDL3main)
    target_link_libraries(iron_gang PRIVATE SDL3::SDL3main)
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
    add_executable(iron_gang_core_tests tests/CoreTests.cpp)
    target_include_directories(iron_gang_core_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
    # Gate M7: lets TestMissionLoadsCommittedFile() validate the real, committed
    # assets/missions/prologue.mission.json (not just a hand-written test fixture), matching
    # iron_gang's own IRON_GANG_DEFAULT_ASSET_DIR convention.
    target_compile_definitions(iron_gang_core_tests PRIVATE
        IRON_GANG_SOURCE_ASSET_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets")
    target_link_libraries(iron_gang_core_tests PRIVATE iron_gang_core)
    iron_gang_link_final_target(iron_gang_core_tests)
    iron_gang_apply_warnings(iron_gang_core_tests)
    add_test(NAME iron_gang_core_tests COMMAND iron_gang_core_tests)
    set_tests_properties(iron_gang_core_tests PROPERTIES
        WORKING_DIRECTORY "${CMAKE_BINARY_DIR}")

    # IG-39-035: the game must fall back to the procedural warehouse box, not crash, when
    # scripts/build-assets.sh has not produced assets/generated/warehouse/cnj/warehouse.cnj.
    add_test(NAME iron_gang_missing_asset_fallback
        COMMAND bash "${CMAKE_CURRENT_SOURCE_DIR}/scripts/test-missing-asset-fallback.sh" "$<TARGET_FILE:iron_gang>")
    set_tests_properties(iron_gang_missing_asset_fallback PROPERTIES
        WORKING_DIRECTORY "${CMAKE_BINARY_DIR}")

    # Gate M4 (IG-39-005): character, trigger, raycast, and vehicle prototypes behind PhysicsWorld.
    add_executable(iron_gang_physics_tests tests/PhysicsTests.cpp)
    target_include_directories(iron_gang_physics_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
    target_link_libraries(iron_gang_physics_tests PRIVATE iron_gang_core)
    iron_gang_link_final_target(iron_gang_physics_tests)
    iron_gang_apply_warnings(iron_gang_physics_tests)
    add_test(NAME iron_gang_physics_tests COMMAND iron_gang_physics_tests)
    set_tests_properties(iron_gang_physics_tests PROPERTIES
        WORKING_DIRECTORY "${CMAKE_BINARY_DIR}")
endif()

install(TARGETS iron_gang RUNTIME DESTINATION bin)
install(DIRECTORY assets/ DESTINATION share/iron-gang/assets)
install(FILES README.md LICENSE THIRD_PARTY.md DESTINATION share/iron-gang)
