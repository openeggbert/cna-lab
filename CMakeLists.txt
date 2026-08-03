cmake_minimum_required(VERSION 3.20)

project(CnaEditor
    VERSION 0.1.0
    DESCRIPTION "Editor, asset pipeline and tooling for the CNA framework"
    LANGUAGES CXX)

# C++23 and the "no compiler extensions" stance both match CNA's own root CMakeLists.txt, so that
# the editor and CNA can be compiled into one build tree without fighting over the standard.
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Build type" FORCE)
endif()

# ---------------------------------------------------------------------------------------------
# Options
# ---------------------------------------------------------------------------------------------

# Default OFF, deliberately. cna-editor-core, -scene, -assets, -project, -ui and -runtime-bridge
# have no CNA dependency at all, and keeping the default build CNA-free is what makes the editor
# clonable and testable on its own -- with no ../cna checkout, no ../sharp-runtime checkout, no
# GPU and no window. See ANALYSIS.md decision D-03.
option(CNA_EDITOR_WITH_CNA "Build the CNA-backed viewport (requires a ../cna sibling checkout)" OFF)

option(CNA_EDITOR_BUILD_TESTS "Build the cna-editor test suite" ON)
option(CNA_EDITOR_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)

set(CNA_EDITOR_CNA_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../cna"
    CACHE PATH "Path to the CNA sibling checkout, used when CNA_EDITOR_WITH_CNA is ON")

# ---------------------------------------------------------------------------------------------
# Shared compile settings
# ---------------------------------------------------------------------------------------------

add_library(cna-editor-settings INTERFACE)
target_compile_features(cna-editor-settings INTERFACE cxx_std_23)
target_include_directories(cna-editor-settings INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_compile_definitions(cna-editor-settings INTERFACE
    CNA_EDITOR_VERSION="${PROJECT_VERSION}")

if(MSVC)
    target_compile_options(cna-editor-settings INTERFACE /W4 /permissive-)
    if(CNA_EDITOR_WARNINGS_AS_ERRORS)
        target_compile_options(cna-editor-settings INTERFACE /WX)
    endif()
else()
    target_compile_options(cna-editor-settings INTERFACE -Wall -Wextra -Wpedantic)
    if(CNA_EDITOR_WARNINGS_AS_ERRORS)
        target_compile_options(cna-editor-settings INTERFACE -Werror)
    endif()
endif()

# ---------------------------------------------------------------------------------------------
# Modules
#
# The split mirrors plan.md's module table. It is enforced by what each target links, not by
# convention: cna-editor-core cannot accidentally acquire a UI dependency, because nothing it
# links has one.
# ---------------------------------------------------------------------------------------------

# cna-editor-core -- identity, JSON, reflection metadata, commands, undo.
# Links nothing but the C++ standard library. Every other module depends on this one; it depends
# on none of them, which is what keeps the document model testable in isolation.
add_library(cna-editor-core STATIC
    src/core/Uuid.cpp
    src/core/Json.cpp
    src/core/PropertyValue.cpp
    src/core/ComponentDescriptor.cpp
    src/core/CommandHistory.cpp)
target_link_libraries(cna-editor-core PUBLIC cna-editor-settings)

# cna-editor-scene -- the scene document, its entities, and every undoable scene operation.
add_library(cna-editor-scene STATIC
    src/scene/EditorEntity.cpp
    src/scene/SceneDocument.cpp
    src/scene/SceneCommands.cpp
    src/scene/BuiltinComponents.cpp)
target_link_libraries(cna-editor-scene PUBLIC cna-editor-core)

# cna-editor-assets -- stable asset identity, sidecars, dependency and reimport tracking.
add_library(cna-editor-assets STATIC
    src/assets/AssetDatabase.cpp)
target_link_libraries(cna-editor-assets PUBLIC cna-editor-core)

# cna-editor-project -- the .cnaproject format and the backend capability table.
add_library(cna-editor-project STATIC
    src/project/Project.cpp)
target_link_libraries(cna-editor-project PUBLIC cna-editor-core)

# cna-editor-ui -- the toolkit-independent UI surface plus its null implementation. No panel or
# plugin may call a toolkit directly; they call this. See ANALYSIS.md decision D-02.
add_library(cna-editor-ui STATIC
    src/ui/NullEditorUi.cpp)
target_link_libraries(cna-editor-ui PUBLIC cna-editor-core)

# cna-editor-runtime-bridge -- the editor/player wire protocol.
add_library(cna-editor-runtime-bridge STATIC
    src/runtime-bridge/EditorProtocol.cpp)
target_link_libraries(cna-editor-runtime-bridge PUBLIC cna-editor-core)

# cna-editor-plugins -- manifest discovery and validation.
add_library(cna-editor-plugins STATIC
    src/plugins/PluginHost.cpp)
target_link_libraries(cna-editor-plugins PUBLIC cna-editor-core)

# cna-editor-context -- the composition layer: one object holding the open project, the open
# scene, the component registry, the asset database, the undo stack and the selection.
#
# It lives above the other modules rather than inside cna-editor-core precisely because it depends
# on all of them. Putting EditorContext in core would make core depend on scene, and scene already
# depends on core -- a cycle. Keeping the hub in its own module is what lets the dependency graph
# stay acyclic and the layering stay checkable by the build system rather than by review.
add_library(cna-editor-context STATIC
    src/context/EditorContext.cpp)
target_link_libraries(cna-editor-context PUBLIC
    cna-editor-scene
    cna-editor-assets
    cna-editor-project
    cna-editor-ui)

# ---------------------------------------------------------------------------------------------
# Optional CNA-backed viewport
# ---------------------------------------------------------------------------------------------

if(CNA_EDITOR_WITH_CNA)
    if(NOT EXISTS "${CNA_EDITOR_CNA_ROOT}/CMakeLists.txt")
        message(FATAL_ERROR
            "CNA_EDITOR_WITH_CNA is ON but no CNA checkout was found at "
            "${CNA_EDITOR_CNA_ROOT}. CNA is a sibling checkout, not a submodule -- and it in turn "
            "needs its own ../sharp-runtime sibling. Fix:\n"
            "  cd ${CMAKE_CURRENT_SOURCE_DIR}/.. && \\\n"
            "  git clone https://github.com/openeggbert/cna.git && \\\n"
            "  git clone https://github.com/openeggbert/sharp-runtime.git\n"
            "Or configure with -DCNA_EDITOR_WITH_CNA=OFF to build the CNA-free editor core.")
    endif()

    message(STATUS "cna-editor: building the CNA-backed viewport from ${CNA_EDITOR_CNA_ROOT}")
    message(STATUS "cna-editor: CNA selects its graphics backend at compile time; this editor "
                   "binary will be fixed to whatever CNA_GRAPHICS_BACKEND is set to")
    add_subdirectory("${CNA_EDITOR_CNA_ROOT}" "${CMAKE_CURRENT_BINARY_DIR}/cna")

    add_library(cna-editor-viewport STATIC
        src/viewport/CnaEditorViewport.cpp)
    target_link_libraries(cna-editor-viewport PUBLIC cna-editor-scene PRIVATE CNA)
    target_compile_definitions(cna-editor-viewport PUBLIC CNA_EDITOR_HAS_CNA=1)
endif()

# ---------------------------------------------------------------------------------------------
# Application
# ---------------------------------------------------------------------------------------------

add_executable(cna-editor
    src/app/Main.cpp
    src/app/EditorApplication.cpp)
target_link_libraries(cna-editor PRIVATE
    cna-editor-context
    cna-editor-plugins
    cna-editor-runtime-bridge)

if(CNA_EDITOR_WITH_CNA)
    target_link_libraries(cna-editor PRIVATE cna-editor-viewport)
endif()

# ---------------------------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------------------------

if(CNA_EDITOR_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

message(STATUS "cna-editor ${PROJECT_VERSION}")
message(STATUS "  CNA viewport : ${CNA_EDITOR_WITH_CNA}")
message(STATUS "  tests        : ${CNA_EDITOR_BUILD_TESTS}")
