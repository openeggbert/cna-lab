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

# Default ON, unlike CNA_EDITOR_WITH_CNA. Dear ImGui is vendored in third_party/ and is pure
# portable C++ with no system dependencies of its own, so building it costs nothing but compile
# time -- and it lets CI run the *real* editor UI headless, asserting on the geometry it produces
# with no window and no GPU. See ANALYSIS.md decision D-14.
option(CNA_EDITOR_WITH_IMGUI "Build the Dear ImGui implementation of EditorUi" ON)

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
    src/scene/BuiltinComponents.cpp
    src/scene/SceneTransform.cpp
    src/scene/EditorCamera2D.cpp
    src/scene/EditorIcons.cpp
    src/scene/MissingReferences.cpp
    src/scene/SceneValidation.cpp
    src/scene/TranslateGizmo.cpp
    src/viewport/NullEditorViewport.cpp)
target_link_libraries(cna-editor-scene PUBLIC cna-editor-core)

# cna-editor-assets -- stable asset identity, sidecars, dependency and reimport tracking.
add_library(cna-editor-assets STATIC
    src/assets/AssetDatabase.cpp
    src/assets/AssetCommands.cpp
    src/assets/AssetWatcher.cpp
    src/assets/AssetImporters.cpp
    src/assets/AssetTree.cpp)
target_link_libraries(cna-editor-assets PUBLIC cna-editor-core)

# cna-editor-project -- the .cnaproject format and the backend capability table.
add_library(cna-editor-project STATIC
    src/project/Project.cpp)
target_link_libraries(cna-editor-project PUBLIC cna-editor-core)

# cna-editor-ui -- the toolkit-independent UI surface plus its null implementation, and the two
# types that carry data across the toolkit boundary: UiDrawData (geometry out) and UiInputState
# (input in). No panel or plugin may call a toolkit directly; they call this. See ANALYSIS.md
# decisions D-02 and D-14.
add_library(cna-editor-ui STATIC
    src/ui/NullEditorUi.cpp
    src/ui/UiDrawData.cpp
    src/ui/UiInputState.cpp)
target_link_libraries(cna-editor-ui PUBLIC cna-editor-core)

# Dear ImGui itself. Vendored core only -- none of ImGui's own platform or renderer backends are
# built, because cna-editor supplies its own through CNA's public API (ANALYSIS.md decision D-14).
if(CNA_EDITOR_WITH_IMGUI)
    add_library(cna-editor-imgui-vendor STATIC
        third_party/imgui/imgui.cpp
        third_party/imgui/imgui_draw.cpp
        third_party/imgui/imgui_tables.cpp
        third_party/imgui/imgui_widgets.cpp)
    target_include_directories(cna-editor-imgui-vendor SYSTEM PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui")
    target_compile_features(cna-editor-imgui-vendor PUBLIC cxx_std_23)
    # Third-party code is not held to this project's warning settings; its warnings are not
    # actionable here and would drown out our own under -Werror.
    if(NOT MSVC)
        target_compile_options(cna-editor-imgui-vendor PRIVATE -w)
    endif()

    # cna-editor-ui-imgui -- the ImGui implementation of EditorUi. Needs ImGui and nothing else:
    # no window system, no graphics API, no CNA. That is what makes it testable headless.
    add_library(cna-editor-ui-imgui STATIC
        src/ui/imgui/ImGuiEditorUi.cpp)
    target_link_libraries(cna-editor-ui-imgui PUBLIC cna-editor-ui PRIVATE cna-editor-imgui-vendor)
endif()

# cna-editor-runtime-bridge -- the editor/player wire protocol, its loopback TCP transport, and
# the editor-side process supervisor. Sockets and process spawning are the only platform-specific
# code in this repository; both are contained here.
add_library(cna-editor-runtime-bridge STATIC
    src/runtime-bridge/EditorProtocol.cpp
    src/runtime-bridge/MessageChannel.cpp
    src/runtime-bridge/PlayerProcess.cpp)
target_link_libraries(cna-editor-runtime-bridge PUBLIC cna-editor-core)
if(WIN32)
    target_link_libraries(cna-editor-runtime-bridge PUBLIC ws2_32)
endif()

# cna-editor-player -- the player-side protocol and state machine. Deliberately CNA-free, so the
# whole message-handling surface is unit-testable headless; the CNA-linked half is cna-player's
# own main loop. See ANALYSIS.md decision D-15.
add_library(cna-editor-player STATIC
    src/player/PlayerHost.cpp)
target_link_libraries(cna-editor-player PUBLIC
    cna-editor-scene
    cna-editor-assets
    cna-editor-project
    cna-editor-runtime-bridge)

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

    # The only module that links CNA. It holds three things: the scene viewport, the UI renderer
    # that draws UiDrawData through CNA's public graphics API, and the platform layer that fills
    # UiInputState from CNA's public input API. See docs/SPIKE-IMGUI-CNA.md for the full mapping.
    set(_cna_editor_viewport_sources
        src/viewport/CnaEditorViewport.cpp
        src/viewport/CnaSceneRenderer.cpp
        src/viewport/CnaUiRenderer.cpp
        src/viewport/CnaUiPlatform.cpp)

    # The window host needs both CNA and Dear ImGui, so it only exists when both are built.
    # Without it the editor still compiles and runs headless -- the CNA-linked renderer and
    # platform layer are independently useful, and the tests exercise them.
    if(CNA_EDITOR_WITH_IMGUI)
        list(APPEND _cna_editor_viewport_sources src/viewport/CnaEditorHost.cpp)
    endif()

    add_library(cna-editor-viewport STATIC ${_cna_editor_viewport_sources})
    target_link_libraries(cna-editor-viewport PUBLIC cna-editor-scene cna-editor-ui PRIVATE CNA)
    target_compile_definitions(cna-editor-viewport PUBLIC CNA_EDITOR_HAS_CNA=1)

    if(CNA_EDITOR_WITH_IMGUI)
        target_link_libraries(cna-editor-viewport PUBLIC cna-editor-context cna-editor-ui-imgui)
        target_compile_definitions(cna-editor-viewport PUBLIC CNA_EDITOR_HAS_HOST=1)
    endif()
endif()

# ---------------------------------------------------------------------------------------------
# Application
# ---------------------------------------------------------------------------------------------

# Listed once and reused by the test target, which builds the panels alongside the application so
# the tests drive the real ones (plan.md ED-210).
set(CNA_EDITOR_PANEL_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/panels/MainMenuBar.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/panels/HierarchyPanel.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/panels/InspectorPanel.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/panels/AssetBrowserPanel.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/panels/ViewportPanel.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/panels/ValidationPanel.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/panels/ConsolePanel.cpp
    CACHE INTERNAL "Editor panel sources")

add_executable(cna-editor
    src/app/Main.cpp
    src/app/EditorApplication.cpp
    ${CNA_EDITOR_PANEL_SOURCES})
target_link_libraries(cna-editor PRIVATE
    cna-editor-context
    cna-editor-plugins
    cna-editor-runtime-bridge)

if(CNA_EDITOR_WITH_CNA)
    target_link_libraries(cna-editor PRIVATE cna-editor-viewport)
endif()

if(CNA_EDITOR_WITH_IMGUI)
    target_link_libraries(cna-editor PRIVATE cna-editor-ui-imgui)
    target_compile_definitions(cna-editor PRIVATE CNA_EDITOR_HAS_IMGUI=1)
endif()

# ---------------------------------------------------------------------------------------------
# scene-loader-demo
#
# What a shipped game does with a scene the editor produced (plan.md ED-250). It is also the
# loader's integration test: it opens a real device, loads examples/HelloSprites, checks the world
# transforms against what the editor's own code produces, and draws a frame.
#
# Only built with CNA, because that is the only configuration in which a game exists at all.
# ---------------------------------------------------------------------------------------------

if(CNA_EDITOR_WITH_CNA)
    add_executable(scene-loader-demo examples/SceneLoaderDemo/Main.cpp)

    # The loader is header-only and needs exactly what its documentation claims: CNA's public API
    # and cna-editor-core. If this ever needs more, the claim has stopped being true.
    target_link_libraries(scene-loader-demo PRIVATE cna-editor-core CNA)
endif()

# ---------------------------------------------------------------------------------------------
# cna-player
#
# Built once per graphics backend in a real installation -- cna-player-easygl,
# cna-player-software, and so on -- because CNA fixes its backend at compile time. The editor
# discovers which builds exist and launches the one matching the backend the user asked to
# preview. Built here without CNA, it still speaks the whole protocol; that configuration is what
# the editor's own bridge tests run against.
# ---------------------------------------------------------------------------------------------

add_executable(cna-player src/player/Main.cpp)
target_link_libraries(cna-player PRIVATE cna-editor-player)

if(CNA_EDITOR_WITH_CNA)
    target_link_libraries(cna-player PRIVATE cna-editor-viewport)
    # The binary is named for its backend so that discovery can tell the builds apart on disk.
    string(TOLOWER "${CNA_GRAPHICS_BACKEND}" _cna_editor_player_suffix)
    set_target_properties(cna-player PROPERTIES OUTPUT_NAME "cna-player-${_cna_editor_player_suffix}")
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
message(STATUS "  Dear ImGui   : ${CNA_EDITOR_WITH_IMGUI}")
message(STATUS "  tests        : ${CNA_EDITOR_BUILD_TESTS}")
