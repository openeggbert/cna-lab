cmake_minimum_required(VERSION 3.20)
# Allow FetchContent dependencies that declare older cmake_minimum_required
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
project(MeshWorldApp LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)

# ---------------------------------------------------------------------------
# Paths — all siblings of mesh-world/
# ---------------------------------------------------------------------------
set(MESH_WORLD_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../..")
set(CNA_ROOT        "${MESH_WORLD_ROOT}/../cna")
set(MESH_CRAFT_ROOT "${MESH_WORLD_ROOT}/../mesh-craft")

if(NOT EXISTS "${CNA_ROOT}/CMakeLists.txt")
    message(FATAL_ERROR "CNA not found at ${CNA_ROOT}. "
            "Clone openeggbert/cna next to mesh-world.")
endif()

# ---------------------------------------------------------------------------
# CNA — must be added BEFORE mc3 so sharp-runtime's tinyxml2 is created
# first. mc3 checks `if(NOT TARGET tinyxml2)` and skips if already present.
# ---------------------------------------------------------------------------
set(CNA_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
set(CNA_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(CNA_BACKEND_SDL_RENDERER OFF CACHE BOOL "" FORCE)
set(CNA_BACKEND_EASY_GL      ON  CACHE BOOL "" FORCE)
set(CNA_GRAPHICS_BACKEND "EASYGL" CACHE STRING "" FORCE)
set(SHARP_RUNTIME_BUILD_TESTS OFF CACHE BOOL "" FORCE)

add_subdirectory("${CNA_ROOT}" CNA_dep)

# ---------------------------------------------------------------------------
# Lua 5.4 (needed by MeshWorldLib)
# ---------------------------------------------------------------------------
FetchContent_Declare(lua
    URL                    https://www.lua.org/ftp/lua-5.4.7.tar.gz
    URL_HASH               SHA256=9fbf5e28ef86c69858f6d3d34eccc32e911c1a28b4120ff3e84aaa70cfbf1e30
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(lua)

file(GLOB LUA_SOURCES "${lua_SOURCE_DIR}/src/*.c")
list(FILTER LUA_SOURCES EXCLUDE REGEX ".*/lua\\.c$")
list(FILTER LUA_SOURCES EXCLUDE REGEX ".*/luac\\.c$")
add_library(lua54 STATIC ${LUA_SOURCES})
target_include_directories(lua54 PUBLIC "${lua_SOURCE_DIR}/src")
if(NOT MSVC)
    target_compile_options(lua54 PRIVATE -w)
endif()

# ---------------------------------------------------------------------------
# sol2 (header-only Lua binding)
# ---------------------------------------------------------------------------
FetchContent_Declare(sol2
    URL      https://github.com/ThePhD/sol2/archive/refs/tags/v3.3.0.tar.gz
    URL_HASH SHA256=b82c5de030e18cb2bcbcefcd5f45afd526920c517a96413f0b59b4332d752a1e
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(sol2)

# ---------------------------------------------------------------------------
# SQLite3
# ---------------------------------------------------------------------------
find_package(SQLite3 QUIET)
if(NOT SQLite3_FOUND)
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
    endif()
    add_library(SQLite::SQLite3 ALIAS sqlite3_lib)
endif()

# ---------------------------------------------------------------------------
# Re-expose SDL3 in this scope.
# CNA's cmake already populated SDL3_DIR in the cache; re-running find_package
# here creates SDL3::SDL3 as a target visible to targets defined in this file.
# ---------------------------------------------------------------------------
if(NOT TARGET SDL3::SDL3)
    find_package(SDL3 CONFIG QUIET)
endif()

# ---------------------------------------------------------------------------
# MeshCraft mc3 — must come AFTER CNA so tinyxml2 target already exists
# ---------------------------------------------------------------------------
set(_BUILD_TESTING_SAVE "${BUILD_TESTING}")
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
add_subdirectory("${MESH_CRAFT_ROOT}/mc3" mc3_dep)
set(BUILD_TESTING "${_BUILD_TESTING_SAVE}" CACHE BOOL "" FORCE)

# ---------------------------------------------------------------------------
# manifold + tinyobjloader (for SceneRenderer)
# ---------------------------------------------------------------------------
FetchContent_Declare(manifold
    GIT_REPOSITORY https://github.com/elalish/manifold.git
    GIT_TAG        v3.0.0
    GIT_SHALLOW    TRUE
)
set(MANIFOLD_CBLAS          OFF  CACHE BOOL   "" FORCE)
set(MANIFOLD_PAR            "NONE" CACHE STRING "" FORCE)
set(MANIFOLD_USE_BUILTIN_TBB OFF  CACHE BOOL   "" FORCE)
set(MANIFOLD_TEST           OFF  CACHE BOOL   "" FORCE)
set(MANIFOLD_EXPORT         OFF  CACHE BOOL   "" FORCE)
FetchContent_MakeAvailable(manifold)

FetchContent_Declare(tinyobjloader
    GIT_REPOSITORY https://github.com/tinyobjloader/tinyobjloader.git
    GIT_TAG        v2.0.0rc13
    GIT_SHALLOW    TRUE
)
set(TINYOBJLOADER_BUILD_TEST_LOADER OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(tinyobjloader)

# ---------------------------------------------------------------------------
# mc3togltf_lib — GltfExporter/MeshBuilder/CsgEvaluator (from mesh-craft).
# SceneRenderer.cpp (below) #includes "MeshBuilder.hpp" and calls its
# buildPrimitive() (STAB-0670/STAB-0722, mesh-craft's own recent addition) --
# a real compiled dependency, not header-only, so this needs both the
# include dir AND the linked library, not just an extra -I. mc3togltf's own
# CMakeLists.txt guards re-fetching manifold/Mc3/tinyobjloader if the target
# already exists (all 3 do, from just above), so this only newly adds
# tinygltf.
# ---------------------------------------------------------------------------
add_subdirectory("${MESH_CRAFT_ROOT}/mc3togltf" mc3togltf_dep)

# ---------------------------------------------------------------------------
# MeshCraftRendererLib — SceneRenderer from mesh-craft
# ---------------------------------------------------------------------------
set(RENDERER_SRC_DIR "${MESH_CRAFT_ROOT}/src/MeshCraft/Renderer")
set(RENDERER_INC_DIR "${MESH_CRAFT_ROOT}/include")

file(GLOB RENDERER_SOURCES "${RENDERER_SRC_DIR}/*.cpp")
add_library(MeshCraftRendererLib STATIC ${RENDERER_SOURCES})
target_include_directories(MeshCraftRendererLib PUBLIC  "${RENDERER_INC_DIR}")
target_include_directories(MeshCraftRendererLib PRIVATE
    "${MESH_CRAFT_ROOT}/src"
    "${tinyobjloader_SOURCE_DIR}"
)
target_link_libraries(MeshCraftRendererLib PUBLIC Mc3 CNA manifold tinyobjloader mc3togltf_lib)
if(NOT MSVC)
    target_compile_options(MeshCraftRendererLib PRIVATE -w)
endif()

# ---------------------------------------------------------------------------
# stb_image_write.h — single-header public-domain PNG/BMP/TGA writer (M205).
# MeshWorldLib's src/PlanetMapLogic.cpp #includes this unconditionally, so
# this separate CMake project needs the same fetch + targets the root
# CMakeLists.txt sets up (mirrored exactly -- same pinned commit/hash).
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

# Implementation in its own translation unit, compiled without
# -Wall -Wextra -Werror -- third-party code doesn't meet this project's own
# warning bar (same treatment as the root CMakeLists.txt's own copy).
add_library(stb_image_write_impl OBJECT "${MESH_WORLD_ROOT}/src/vendor/stb_image_write_impl.cpp")
target_include_directories(stb_image_write_impl PRIVATE "${STB_IMAGE_WRITE_DIR}")
if(NOT MSVC)
    target_compile_options(stb_image_write_impl PRIVATE -w)
else()
    target_compile_options(stb_image_write_impl PRIVATE /w)
endif()

# ---------------------------------------------------------------------------
# MeshWorldLib sources (compiled here with MESH_WORLD_HAS_RENDERER defined)
# ---------------------------------------------------------------------------
file(GLOB_RECURSE MESH_WORLD_SOURCES CONFIGURE_DEPENDS
    "${MESH_WORLD_ROOT}/src/*.cpp"
)
list(FILTER MESH_WORLD_SOURCES EXCLUDE REGEX ".*/main\\.cpp$")
list(FILTER MESH_WORLD_SOURCES EXCLUDE REGEX ".*/tools/.*\\.cpp$")
# stb_image_write_impl.cpp is its own OBJECT library target above, compiled
# without this project's strict warning flags -- exclude it here so it isn't
# also compiled (a second time, under -Wall -Wextra -Werror) as part of
# MeshWorldLibApp's own glob.
list(FILTER MESH_WORLD_SOURCES EXCLUDE REGEX ".*/src/vendor/stb_image_write_impl\\.cpp$")

add_library(MeshWorldLibApp STATIC ${MESH_WORLD_SOURCES})
target_include_directories(MeshWorldLibApp PUBLIC
    "${MESH_WORLD_ROOT}/include"
    "${MESH_WORLD_ROOT}/vendor"
    "${RENDERER_INC_DIR}"
)
target_compile_definitions(MeshWorldLibApp PUBLIC MESH_WORLD_HAS_RENDERER)
target_link_libraries(MeshWorldLibApp PUBLIC
    Mc3 lua54 sol2 SQLite::SQLite3 tinyxml2 MeshCraftRendererLib
    stb_image_write stb_image_write_impl)
if(NOT MSVC)
    target_compile_options(MeshWorldLibApp PRIVATE -Wall -Wextra -Werror)
endif()

# ---------------------------------------------------------------------------
# ImGui (with SDL3 + OpenGL ES3 backends)
# ---------------------------------------------------------------------------
FetchContent_Declare(imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        v1.91.9
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(imgui)

add_library(imgui_lib STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl3.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)
target_include_directories(imgui_lib PUBLIC
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)
# CNA's EasyGL backend uses OpenGL ES 3.0; match ImGui's loader to ES3.
target_compile_definitions(imgui_lib PUBLIC IMGUI_IMPL_OPENGL_ES3)
target_link_libraries(imgui_lib PUBLIC SDL3::SDL3 GLESv2)
if(NOT MSVC)
    target_compile_options(imgui_lib PRIVATE -w)
endif()

# ---------------------------------------------------------------------------
# MeshWorldApp executable
# ---------------------------------------------------------------------------
add_executable(MeshWorldApp main.cpp)
target_link_libraries(MeshWorldApp PRIVATE MeshWorldLibApp CNA imgui_lib)
