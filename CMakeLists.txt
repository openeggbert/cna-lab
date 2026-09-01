cmake_minimum_required(VERSION 3.20)

project(MESH_WORLD LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(MESH_WORLD_BUILD_TESTS "Build MeshWorld tests" ON)

include(FetchContent)

# ---------------------------------------------------------------------------
# Lua 5.4 — build from source as a static library.
# liblua5.4-dev is not installed on the system; FetchContent builds from src.
# ---------------------------------------------------------------------------
FetchContent_Declare(lua
    URL                    https://www.lua.org/ftp/lua-5.4.7.tar.gz
    URL_HASH               SHA256=9fbf5e28ef86c69858f6d3d34eccc32e911c1a28b4120ff3e84aaa70cfbf1e30
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(lua)

file(GLOB LUA_SOURCES "${lua_SOURCE_DIR}/src/*.c")
# Exclude interpreter and compiler mains (both define main())
list(FILTER LUA_SOURCES EXCLUDE REGEX ".*/lua\\.c$")
list(FILTER LUA_SOURCES EXCLUDE REGEX ".*/luac\\.c$")

add_library(lua54 STATIC ${LUA_SOURCES})
target_include_directories(lua54 PUBLIC "${lua_SOURCE_DIR}/src")
# Suppress warnings in Lua's own C sources — they are not our code
if(NOT MSVC)
    target_compile_options(lua54 PRIVATE -w)
else()
    target_compile_options(lua54 PRIVATE /w)
endif()

# ---------------------------------------------------------------------------
# sol2 v3.3.0 — header-only C++ Lua binding (no CMake sub-build needed)
# ---------------------------------------------------------------------------
FetchContent_Declare(sol2
    URL                    https://github.com/ThePhD/sol2/archive/refs/tags/v3.3.0.tar.gz
    URL_HASH               SHA256=b82c5de030e18cb2bcbcefcd5f45afd526920c517a96413f0b59b4332d752a1e
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(sol2)
# sol2's CMakeLists creates the sol2::sol2 INTERFACE target automatically.

# ---------------------------------------------------------------------------
# SQLite3 — use system package; fall back to amalgamation via FetchContent.
# ---------------------------------------------------------------------------
find_package(SQLite3 QUIET)
if(NOT SQLite3_FOUND)
    message(STATUS "System SQLite3 not found — fetching amalgamation via FetchContent")
    FetchContent_Declare(sqlite3_amal
        URL      https://www.sqlite.org/2024/sqlite-amalgamation-3470100.zip
        URL_HASH SHA256=5af07b2e253e7e5066ca3ec9de8a5f7e02958ffe1fcca40b2e1f8bff8e84dc93
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    FetchContent_MakeAvailable(sqlite3_amal)
    add_library(sqlite3_lib STATIC "${sqlite3_amal_SOURCE_DIR}/sqlite3.c")
    target_include_directories(sqlite3_lib PUBLIC "${sqlite3_amal_SOURCE_DIR}")
    if(NOT MSVC)
        target_compile_options(sqlite3_lib PRIVATE -w)
    else()
        target_compile_options(sqlite3_lib PRIVATE /w)
    endif()
    add_library(SQLite::SQLite3 ALIAS sqlite3_lib)
endif()

# ---------------------------------------------------------------------------
# stb_image_write.h — single-header public-domain PNG/BMP/TGA writer (M205).
# Fetched as a lone file (not a git checkout of the whole stb repo) via
# file(DOWNLOAD ...), pinned to the commit that last touched this header.
# ---------------------------------------------------------------------------
set(STB_IMAGE_WRITE_DIR "${CMAKE_BINARY_DIR}/_deps/stb_image_write")
set(STB_IMAGE_WRITE_HEADER "${STB_IMAGE_WRITE_DIR}/stb_image_write.h")
if(NOT EXISTS "${STB_IMAGE_WRITE_HEADER}")
    file(DOWNLOAD
        "https://raw.githubusercontent.com/nothings/stb/1ee679ca2ef753a528db5ba6801e1067b40481b8/stb_image_write.h"
        "${STB_IMAGE_WRITE_HEADER}"
        EXPECTED_HASH SHA256=cbd5f0ad7a9cf4468affb36354a1d2338034f2c12473cf1a8e32053cb6914a05
    )
endif()
add_library(stb_image_write INTERFACE)
target_include_directories(stb_image_write INTERFACE "${STB_IMAGE_WRITE_DIR}")

# The implementation (STB_IMAGE_WRITE_IMPLEMENTATION) lives in its own
# translation unit, compiled without -Wall -Wextra -Werror -- third-party
# code doesn't meet this project's own warning bar (same treatment as
# lua54/sqlite3_lib above).
add_library(stb_image_write_impl OBJECT src/vendor/stb_image_write_impl.cpp)
target_include_directories(stb_image_write_impl PRIVATE "${STB_IMAGE_WRITE_DIR}")
if(NOT MSVC)
    target_compile_options(stb_image_write_impl PRIVATE -w)
else()
    target_compile_options(stb_image_write_impl PRIVATE /w)
endif()

# ---------------------------------------------------------------------------
# earcut.hpp — single-header, ISC-licensed polygon triangulation (M+fix,
# render_png_biome_image()'s Border-polygon fill). Fetched as a lone file via
# file(DOWNLOAD ...), pinned to a specific commit, same treatment as
# stb_image_write.h above. Pure header templates -- no separate
# IMPLEMENTATION translation unit needed, so no -Wall/-Werror carve-out.
# Vendored at _deps/earcut/mapbox/earcut.hpp (not flattened) so callers can
# write #include <mapbox/earcut.hpp>, matching earcut's own upstream layout.
# ---------------------------------------------------------------------------
set(EARCUT_ROOT_DIR "${CMAKE_BINARY_DIR}/_deps/earcut")
set(EARCUT_HEADER "${EARCUT_ROOT_DIR}/mapbox/earcut.hpp")
if(NOT EXISTS "${EARCUT_HEADER}")
    file(DOWNLOAD
        "https://raw.githubusercontent.com/mapbox/earcut.hpp/f25bc765e3084583b7350080319c29ad87bf5857/include/mapbox/earcut.hpp"
        "${EARCUT_HEADER}"
        EXPECTED_HASH SHA256=b5df93e4866beeff53c1e5f6ba95471669c85e7be9559aa96082c9f3f749f503
    )
endif()
add_library(earcut INTERFACE)
target_include_directories(earcut INTERFACE "${EARCUT_ROOT_DIR}")

# ---------------------------------------------------------------------------
# MeshCraft Mc3 library — canonical MC3 document model + XML writer
# ---------------------------------------------------------------------------

set(MC3_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../mesh-craft/mc3")
if(NOT EXISTS "${MC3_SOURCE_DIR}/CMakeLists.txt")
    message(FATAL_ERROR "MeshCraft mc3 not found at ${MC3_SOURCE_DIR}. "
            "Please clone openeggbert/mesh-craft next to mesh-world.")
endif()

# Disable mc3's own tests to avoid pulling in its test binary.
set(_BUILD_TESTING_SAVE "${BUILD_TESTING}")
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
add_subdirectory("${MC3_SOURCE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/mc3_dep")
set(BUILD_TESTING "${_BUILD_TESTING_SAVE}" CACHE BOOL "" FORCE)

# ---------------------------------------------------------------------------
# MeshCraft Mcb library — MCB binary format writer/reader (for --mcb flag)
# ---------------------------------------------------------------------------

set(MCB_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../mesh-craft/mcb")
if(EXISTS "${MCB_SOURCE_DIR}/CMakeLists.txt")
    add_subdirectory("${MCB_SOURCE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/mcb_dep")
    set(MESH_WORLD_HAS_MCB TRUE)
else()
    message(STATUS "MeshCraft mcb not found at ${MCB_SOURCE_DIR} — MeshWorldExport --mcb disabled")
    set(MESH_WORLD_HAS_MCB FALSE)
endif()

# ---------------------------------------------------------------------------
# MeshCraft mc3togltf library — glTF/GLB export (for MeshWorldGLB, T241)
# ---------------------------------------------------------------------------

set(MC3TOGLTF_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../mesh-craft/mc3togltf")
if(EXISTS "${MC3TOGLTF_SOURCE_DIR}/CMakeLists.txt")
    set(_BUILD_TESTING_SAVE2 "${BUILD_TESTING}")
    set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
    add_subdirectory("${MC3TOGLTF_SOURCE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/mc3togltf_dep")
    set(BUILD_TESTING "${_BUILD_TESTING_SAVE2}" CACHE BOOL "" FORCE)
    set(MESH_WORLD_HAS_GLTF TRUE)
else()
    message(STATUS "MeshCraft mc3togltf not found at ${MC3TOGLTF_SOURCE_DIR} — MeshWorldGLB disabled")
    set(MESH_WORLD_HAS_GLTF FALSE)
endif()

# ---------------------------------------------------------------------------
# MeshWorldLib
# ---------------------------------------------------------------------------

file(GLOB_RECURSE MESH_WORLD_SOURCES CONFIGURE_DEPENDS "src/*.cpp")
list(FILTER MESH_WORLD_SOURCES EXCLUDE REGEX ".*/main\\.cpp$")
list(FILTER MESH_WORLD_SOURCES EXCLUDE REGEX ".*/tools/.*\\.cpp$")
# Mc3Renderer.cpp is compiled only when MESH_WORLD_HAS_RENDERER is set externally
# (e.g. from apps/mesh-world-app/CMakeLists.txt which owns CNA + SceneRenderer).
list(FILTER MESH_WORLD_SOURCES EXCLUDE REGEX ".*/Mc3Renderer\\.cpp$")
# stb_image_write_impl.cpp is its own OBJECT library target above, compiled
# without -Werror -- exclude it from MeshWorldLib's own strict-warnings glob.
list(FILTER MESH_WORLD_SOURCES EXCLUDE REGEX ".*/src/vendor/stb_image_write_impl\\.cpp$")

add_library(MeshWorldLib STATIC ${MESH_WORLD_SOURCES})

target_include_directories(MeshWorldLib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/vendor
)

target_link_libraries(MeshWorldLib PUBLIC Mc3 lua54 sol2 SQLite::SQLite3 tinyxml2 stb_image_write stb_image_write_impl earcut)

if(MSVC)
    target_compile_options(MeshWorldLib PRIVATE /W4 /WX)
else()
    target_compile_options(MeshWorldLib PRIVATE -Wall -Wextra -Werror)
endif()

add_executable(MeshWorld src/main.cpp)
target_link_libraries(MeshWorld PRIVATE MeshWorldLib)

add_executable(MeshWorldExport src/tools/export_chunks.cpp)
target_link_libraries(MeshWorldExport PRIVATE MeshWorldLib)
if(MESH_WORLD_HAS_MCB)
    target_link_libraries(MeshWorldExport PRIVATE Mcb)
    target_compile_definitions(MeshWorldExport PRIVATE MESH_WORLD_HAS_MCB)
endif()

add_executable(MeshWorldMap src/tools/print_map.cpp)
target_link_libraries(MeshWorldMap PRIVATE MeshWorldLib)

add_executable(MeshWorldMaterials src/tools/print_materials.cpp)
target_link_libraries(MeshWorldMaterials PRIVATE MeshWorldLib)

add_executable(MeshWorldPack src/tools/pack_content.cpp)
target_link_libraries(MeshWorldPack PRIVATE MeshWorldLib)

add_executable(MeshWorldPlanet src/tools/planet_map.cpp)
target_link_libraries(MeshWorldPlanet PRIVATE MeshWorldLib)

add_executable(MeshWorldValidate src/tools/validate_mc3.cpp)
target_link_libraries(MeshWorldValidate PRIVATE MeshWorldLib)

add_executable(MeshWorldBuildMc3Lib src/tools/build_mc3lib_content.cpp)
target_link_libraries(MeshWorldBuildMc3Lib PRIVATE MeshWorldLib)

# R132 -- extract one standalone MC3/MCB asset from a reusable mc3lib,
# retaining only its transitive definition/resource dependency graph.
add_executable(MeshWorldPruneMc3Lib src/tools/prune_mc3lib.cpp)
target_link_libraries(MeshWorldPruneMc3Lib PRIVATE MeshWorldLib)
if(MESH_WORLD_HAS_MCB)
    target_link_libraries(MeshWorldPruneMc3Lib PRIVATE Mcb)
    target_compile_definitions(MeshWorldPruneMc3Lib PRIVATE MESH_WORLD_HAS_MCB)
endif()

if(MESH_WORLD_HAS_GLTF)
    add_executable(MeshWorldGLB src/tools/export_glb.cpp)
    target_link_libraries(MeshWorldGLB PRIVATE MeshWorldLib mc3togltf_lib)
endif()

if(MESH_WORLD_BUILD_TESTS)
    if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/vendor/googletest/CMakeLists.txt")
        message(STATUS "vendor/googletest not found — skipping tests.")
        set(MESH_WORLD_BUILD_TESTS OFF)
    endif()
endif()

if(MESH_WORLD_BUILD_TESTS)
    add_subdirectory(vendor/googletest)
    enable_testing()

    file(GLOB_RECURSE MESH_WORLD_TEST_SOURCES CONFIGURE_DEPENDS "tests/*.cpp")

    add_executable(MeshWorldTests ${MESH_WORLD_TEST_SOURCES})
    target_link_libraries(MeshWorldTests PUBLIC MeshWorldLib PRIVATE gtest_main)

    if(NOT MSVC)
        target_compile_options(MeshWorldTests PRIVATE -Wno-unused-result)
    endif()

    include(GoogleTest)
    gtest_discover_tests(MeshWorldTests WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
endif()
