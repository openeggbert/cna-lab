# SPDX-License-Identifier: MS-PL
#
# One binary rather than one per module: the suite is fast, and a single binary keeps
# `ctest --output-on-failure` output readable. Individual suites are registered separately so a
# failure names the area rather than just "CnaEditorTests".

add_executable(cna-editor-tests
    TestMain.cpp
    CoreTests.cpp
    SceneTests.cpp
    CommandTests.cpp
    ProjectAndAssetTests.cpp
    ApplicationTests.cpp
    UiTests.cpp
    PlayerTests.cpp
    ViewportTests.cpp)

target_include_directories(cna-editor-tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(cna-editor-tests PRIVATE
    cna-editor-context
    cna-editor-plugins
    cna-editor-runtime-bridge
    cna-editor-player)

# PlayerTests spawns the real cna-player binary, so it must exist before the suite runs -- and
# the suite must know where it landed. A generator expression over the real target is exact;
# guessing from the test binary's own directory would silently turn the end-to-end bridge test
# into a no-op that always passes.
add_dependencies(cna-editor-tests cna-player)
target_compile_definitions(cna-editor-tests PRIVATE
    CNA_EDITOR_TEST_PLAYER_DIR="$<TARGET_FILE_DIR:cna-player>")

# The application's panel layer is exercised end-to-end by ApplicationTests.cpp, so the test
# binary compiles EditorApplication.cpp directly rather than linking the cna-editor executable.
target_sources(cna-editor-tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../src/app/EditorApplication.cpp")

if(CNA_EDITOR_WITH_IMGUI)
    # The ImGui cases run the real editor UI headless and assert on the geometry it produces.
    target_link_libraries(cna-editor-tests PRIVATE cna-editor-ui-imgui)
    target_compile_definitions(cna-editor-tests PRIVATE CNA_EDITOR_HAS_IMGUI=1)
endif()

add_test(NAME CnaEditorTests COMMAND cna-editor-tests)
set_tests_properties(CnaEditorTests PROPERTIES TIMEOUT 120)

# A smoke test over the real binary: parses arguments, opens nothing, draws one headless frame.
# This is what catches a linker or start-up regression that the unit tests cannot see.
add_test(NAME CnaEditorHeadlessSmoke COMMAND cna-editor --headless)
set_tests_properties(CnaEditorHeadlessSmoke PROPERTIES TIMEOUT 60)

add_test(NAME CnaEditorHelp COMMAND cna-editor --help)
add_test(NAME CnaEditorListBackends COMMAND cna-editor --list-backends)

# --graphics must fail: CNA fixes its backend at compile time, so silently accepting the flag
# would teach a mental model the framework does not support.
add_test(NAME CnaEditorRejectsGraphicsFlag COMMAND cna-editor --graphics=vulkan)
set_tests_properties(CnaEditorRejectsGraphicsFlag PROPERTIES WILL_FAIL TRUE)

add_test(NAME CnaPlayerHelp COMMAND cna-player --help)

if(CNA_EDITOR_WITH_CNA AND CNA_EDITOR_WITH_IMGUI)
    # The real thing: open a window, draw the editor through CNA's public API, and write a PNG of
    # the result. A run that merely exits cleanly cannot tell a working editor from one that opened
    # a blank window, so the screenshot is the assertion -- if the editor drew nothing, the file
    # never appears and the test fails.
    #
    add_test(NAME CnaEditorWindowSmoke
        COMMAND cna-editor
            --project=${CMAKE_CURRENT_SOURCE_DIR}/../examples/HelloSprites/HelloSprites.cnaproject
            --frames=20
            --screenshot=${CMAKE_CURRENT_BINARY_DIR}/editor-smoke.png)

    # How this test reaches a display depends on the backend, and getting it wrong makes the test
    # fail for reasons that have nothing to do with the editor.
    #
    # SOFTWARE and HEADLESS render without a window at all, so SDL's dummy video driver is enough
    # and the test runs anywhere. Every other backend needs a real graphics context: the dummy
    # driver cannot provide one, and forcing it there produces a failure that looks like a bug in
    # the editor and is not. Those get a DISPLAY instead, following the same CNA_TEST_DISPLAY
    # convention CNA itself uses for its window-creating ctests -- point it at an Xvfb server on a
    # build machine with no desktop.
    if(CNA_GRAPHICS_BACKEND STREQUAL "SOFTWARE" OR CNA_GRAPHICS_BACKEND STREQUAL "HEADLESS")
        set_tests_properties(CnaEditorWindowSmoke PROPERTIES
            ENVIRONMENT "SDL_VIDEODRIVER=dummy"
            TIMEOUT 180)
    else()
        set(CNA_EDITOR_TEST_DISPLAY ":0" CACHE STRING
            "DISPLAY used by cna-editor ctests that need a real graphics context")
        set_tests_properties(CnaEditorWindowSmoke PROPERTIES
            ENVIRONMENT "DISPLAY=${CNA_EDITOR_TEST_DISPLAY}"
            LABELS "needs-display"
            TIMEOUT 180)
    endif()

    # --screenshot without --frames has no final frame to capture, and silently writing nothing
    # would make the smoke test above pass for the wrong reason.
    add_test(NAME CnaEditorScreenshotNeedsFrameLimit
        COMMAND cna-editor
            --project=${CMAKE_CURRENT_SOURCE_DIR}/../examples/HelloSprites/HelloSprites.cnaproject
            --screenshot=${CMAKE_CURRENT_BINARY_DIR}/never-written.png
            --headless)
    set_tests_properties(CnaEditorScreenshotNeedsFrameLimit PROPERTIES TIMEOUT 60)
endif()

# cna-player requires a project: launching one without is a mistake worth failing on, not
# something to paper over with a default.
add_test(NAME CnaPlayerRequiresAProject COMMAND cna-player)
set_tests_properties(CnaPlayerRequiresAProject PROPERTIES WILL_FAIL TRUE)
