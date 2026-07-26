cmake_minimum_required(VERSION 3.24)

project(IronShadows
    VERSION 0.1.0
    DESCRIPTION "Original open-city action-adventure prototype built on CNA and sharp-runtime"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(IRON_SHADOWS_BUILD_TESTS "Build Iron Shadows core tests" ON)
option(IRON_SHADOWS_WARNINGS_AS_ERRORS "Treat Iron Shadows warnings as errors" OFF)
option(IRON_SHADOWS_ENABLE_SANITIZERS "Enable AddressSanitizer and UndefinedBehaviorSanitizer" OFF)

set(IRON_SHADOWS_CNA_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../cna" CACHE PATH
    "Path to the CNA source tree. CNA expects sharp-runtime next to itself.")

if(NOT EXISTS "${IRON_SHADOWS_CNA_DIR}/CMakeLists.txt")
    message(FATAL_ERROR
        "Iron Shadows: CNA was not found at '${IRON_SHADOWS_CNA_DIR}'.\n"
        "Recommended workspace layout:\n"
        "  workspace/cna\n"
        "  workspace/sharp-runtime\n"
        "  workspace/easy-gl       # for EASYGL\n"
        "  workspace/iron-shadows\n"
        "Override with -DIRON_SHADOWS_CNA_DIR=/absolute/path/to/cna")
endif()

get_filename_component(IRON_SHADOWS_DEPENDENCY_ROOT "${IRON_SHADOWS_CNA_DIR}/.." ABSOLUTE)
if(NOT EXISTS "${IRON_SHADOWS_DEPENDENCY_ROOT}/sharp-runtime/CMakeLists.txt")
    message(FATAL_ERROR
        "Iron Shadows: sharp-runtime must be a sibling of CNA at\n"
        "  ${IRON_SHADOWS_DEPENDENCY_ROOT}/sharp-runtime\n"
        "This is required by CNA's current build contract.")
endif()

# Consumer-side choices. The project does not build dependency tests/examples by default.
set(CNA_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(CNA_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(CNA_ENABLE_NET OFF CACHE BOOL "" FORCE)
set(CNA_DEVICES OFF CACHE BOOL "" FORCE)
set(SHARP_RUNTIME_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(SHARP_RUNTIME_BUILD_BENCHMARKS OFF CACHE BOOL "" FORCE)

# EASYGL is the recommended prototype backend. Override with VULKAN where available.
if(NOT DEFINED CNA_GRAPHICS_BACKEND)
    set(CNA_GRAPHICS_BACKEND "EASYGL" CACHE STRING "CNA graphics backend")
endif()

add_subdirectory("${IRON_SHADOWS_CNA_DIR}" "${CMAKE_BINARY_DIR}/_deps/cna")

include(cmake/CompilerWarnings.cmake)
include(cmake/CnaLinking.cmake)

add_library(iron_shadows_core STATIC
    src/Dialogue/DialogueSystem.cpp
    src/Gameplay/PlayerController.cpp
    src/Gameplay/VehicleController.cpp
    src/Missions/PrototypeMission.cpp
    src/Persistence/SaveGame.cpp
    src/World/PrototypeWorld.cpp)

add_library(IronShadows::Core ALIAS iron_shadows_core)

target_include_directories(iron_shadows_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(iron_shadows_core
    PUBLIC
        CNA
        SHARP_RUNTIME)

iron_shadows_apply_warnings(iron_shadows_core)

add_executable(iron_shadows
    src/Application/IronShadowsGame.cpp
    src/Graphics/PrimitiveMesh.cpp
    src/Graphics/PrototypeRenderer.cpp
    src/main.cpp)

add_executable(IronShadows::Game ALIAS iron_shadows)

target_include_directories(iron_shadows
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_compile_definitions(iron_shadows PRIVATE
    IRON_SHADOWS_VERSION="${PROJECT_VERSION}"
    IRON_SHADOWS_DEFAULT_ASSET_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets")

target_link_libraries(iron_shadows PRIVATE iron_shadows_core)
iron_shadows_link_final_target(iron_shadows)
iron_shadows_apply_warnings(iron_shadows)

if(TARGET SDL3::SDL3main)
    target_link_libraries(iron_shadows PRIVATE SDL3::SDL3main)
endif()

if(WIN32)
    set_target_properties(iron_shadows PROPERTIES WIN32_EXECUTABLE TRUE)
    if(COMMAND cna_copy_sdl_runtime)
        cna_copy_sdl_runtime(iron_shadows)
    endif()
endif()

set_target_properties(iron_shadows PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")

if(IRON_SHADOWS_ENABLE_SANITIZERS AND NOT MSVC)
    foreach(target iron_shadows_core iron_shadows)
        target_compile_options(${target} PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
        target_link_options(${target} PRIVATE -fsanitize=address,undefined)
    endforeach()
endif()

if(IRON_SHADOWS_BUILD_TESTS)
    enable_testing()
    add_executable(iron_shadows_core_tests tests/CoreTests.cpp)
    target_include_directories(iron_shadows_core_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
    target_link_libraries(iron_shadows_core_tests PRIVATE iron_shadows_core)
    iron_shadows_link_final_target(iron_shadows_core_tests)
    iron_shadows_apply_warnings(iron_shadows_core_tests)
    add_test(NAME iron_shadows_core_tests COMMAND iron_shadows_core_tests)
    set_tests_properties(iron_shadows_core_tests PROPERTIES
        WORKING_DIRECTORY "${CMAKE_BINARY_DIR}")
endif()

install(TARGETS iron_shadows RUNTIME DESTINATION bin)
install(DIRECTORY assets/ DESTINATION share/iron-shadows/assets)
install(FILES README.md LICENSE THIRD_PARTY.md DESTINATION share/iron-shadows)
