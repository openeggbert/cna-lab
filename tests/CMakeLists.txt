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
    ApplicationTests.cpp)

target_include_directories(cna-editor-tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(cna-editor-tests PRIVATE
    cna-editor-context
    cna-editor-plugins
    cna-editor-runtime-bridge)

# The application's panel layer is exercised end-to-end by ApplicationTests.cpp, so the test
# binary compiles EditorApplication.cpp directly rather than linking the cna-editor executable.
target_sources(cna-editor-tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../src/app/EditorApplication.cpp")

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
