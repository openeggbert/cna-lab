cmake_minimum_required(VERSION 3.23)

project(People VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(CTest)

get_filename_component(PEOPLE_CNA_ROOT
    "${CMAKE_CURRENT_SOURCE_DIR}/../cnanext" ABSOLUTE)
get_filename_component(PEOPLE_SHARP_RUNTIME_ROOT
    "${CMAKE_CURRENT_SOURCE_DIR}/../sharp-runtimenext" ABSOLUTE)

set(PEOPLE_CNA_ROOT "${PEOPLE_CNA_ROOT}" CACHE PATH
    "Path to the newer CNA checkout used by People")
set(PEOPLE_SHARP_RUNTIME_ROOT "${PEOPLE_SHARP_RUNTIME_ROOT}" CACHE PATH
    "Path to the newer sharp-runtime checkout used by People")

if(NOT EXISTS "${PEOPLE_CNA_ROOT}/CMakeLists.txt")
    message(FATAL_ERROR
        "People requires the newer CNA checkout at '${PEOPLE_CNA_ROOT}'. "
        "Set PEOPLE_CNA_ROOT to the cnanext checkout.")
endif()
if(NOT EXISTS "${PEOPLE_SHARP_RUNTIME_ROOT}/CMakeLists.txt")
    message(FATAL_ERROR
        "People requires the newer sharp-runtime checkout at "
        "'${PEOPLE_SHARP_RUNTIME_ROOT}'. Set PEOPLE_SHARP_RUNTIME_ROOT to "
        "the sharp-runtimenext checkout.")
endif()

# CNA's supported consumer path is add_subdirectory plus its aggregate CNA
# target. Override its older sibling default before entering the dependency.
set(CNA_SHARP_RUNTIME_ROOT "${PEOPLE_SHARP_RUNTIME_ROOT}" CACHE PATH
    "sharp-runtime checkout consumed by CNA" FORCE)
set(CNA_BUILD_TESTS OFF CACHE BOOL "Build CNA tests" FORCE)
set(CNA_BUILD_EXAMPLES OFF CACHE BOOL "Build CNA examples" FORCE)
set(CNA_ENABLE_NET OFF CACHE BOOL "Build CNA networking" FORCE)
set(CNA_ENABLE_VIDEO OFF CACHE STRING "Build CNA video support" FORCE)
set(SHARP_RUNTIME_BUILD_TESTS OFF CACHE BOOL "Build sharp-runtime tests" FORCE)

if(NOT DEFINED CNA_AUDIO_PLATFORM)
    set(CNA_AUDIO_PLATFORM NULL CACHE STRING
        "CNA audio platform for the current People milestone")
endif()

# CNA builds vendored SDL at configure time. Keep the persistent cache writable
# and outside both read-only dependency checkouts.
if(NOT DEFINED CNA_SDL_PREBUILT_ROOT)
    set(CNA_SDL_PREBUILT_ROOT
        "${CMAKE_CURRENT_SOURCE_DIR}/.cache/cna-sdl-${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}"
        CACHE PATH "Persistent CNA SDL build cache")
endif()

add_subdirectory("${PEOPLE_CNA_ROOT}" "${CMAKE_BINARY_DIR}/_deps/cna")

add_library(people_core STATIC
    game/src/Content/DemoFurniture.cpp
    game/src/Content/DemoResident.cpp
    game/src/Navigation/StaticNavigationGrid.cpp
    game/src/Objects/ObjectModel.cpp
    game/src/Rendering/ObjectPresentation.cpp
    game/src/Rendering/ResidentPresentation.cpp
    game/src/Rendering/RenderOrder.cpp
    game/src/Rendering/WallPresentation.cpp
    game/src/Simulation/ResidentModel.cpp
    game/src/World/IsometricProjection.cpp
    game/src/World/LotGrid.cpp
    game/src/World/RoomMap.cpp
)
target_include_directories(people_core PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/game/include"
)
target_compile_features(people_core PUBLIC cxx_std_23)

add_executable(People
    game/src/PeopleGame.cpp
    game/src/Program.cpp
)
target_include_directories(People PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/game/include"
)
target_link_libraries(People PRIVATE people_core CNA)

if(COMMAND cna_copy_renderer_runtime)
    cna_copy_renderer_runtime(People)
endif()
if(TARGET SDL3::SDL3main)
    target_link_libraries(People PRIVATE SDL3::SDL3main)
endif()

if(MSVC)
    target_compile_options(people_core PRIVATE /W4 /permissive-)
    target_compile_options(People PRIVATE /W4 /permissive-)
else()
    target_compile_options(people_core PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
    target_compile_options(People PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
endif()

if(BUILD_TESTING)
    add_executable(people_core_tests
        tests/IsometricProjectionTests.cpp
    )
    target_link_libraries(people_core_tests PRIVATE people_core)
    if(MSVC)
        target_compile_options(people_core_tests PRIVATE /W4 /permissive-)
    else()
        target_compile_options(people_core_tests PRIVATE
            -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
    endif()
    add_test(NAME people_core_tests COMMAND people_core_tests)

    add_executable(people_render_order_tests
        tests/RenderOrderTests.cpp
    )
    target_link_libraries(people_render_order_tests PRIVATE people_core)
    if(MSVC)
        target_compile_options(people_render_order_tests PRIVATE /W4 /permissive-)
    else()
        target_compile_options(people_render_order_tests PRIVATE
            -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
    endif()
    add_test(NAME people_render_order_tests COMMAND people_render_order_tests)

    add_executable(people_lot_grid_tests
        tests/LotGridTests.cpp
    )
    target_link_libraries(people_lot_grid_tests PRIVATE people_core)
    if(MSVC)
        target_compile_options(people_lot_grid_tests PRIVATE /W4 /permissive-)
    else()
        target_compile_options(people_lot_grid_tests PRIVATE
            -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
    endif()
    add_test(NAME people_lot_grid_tests COMMAND people_lot_grid_tests)

    add_executable(people_object_model_tests
        tests/ObjectModelTests.cpp
    )
    target_link_libraries(people_object_model_tests PRIVATE people_core)
    if(MSVC)
        target_compile_options(people_object_model_tests PRIVATE /W4 /permissive-)
    else()
        target_compile_options(people_object_model_tests PRIVATE
            -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
    endif()
    add_test(NAME people_object_model_tests COMMAND people_object_model_tests)

    add_executable(people_object_presentation_tests
        tests/ObjectPresentationTests.cpp
    )
    target_link_libraries(people_object_presentation_tests PRIVATE people_core)
    if(MSVC)
        target_compile_options(people_object_presentation_tests PRIVATE /W4 /permissive-)
    else()
        target_compile_options(people_object_presentation_tests PRIVATE
            -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
    endif()
    add_test(NAME people_object_presentation_tests COMMAND people_object_presentation_tests)

    add_executable(people_demo_furniture_tests
        tests/DemoFurnitureTests.cpp
    )
    target_link_libraries(people_demo_furniture_tests PRIVATE people_core)
    if(MSVC)
        target_compile_options(people_demo_furniture_tests PRIVATE /W4 /permissive-)
    else()
        target_compile_options(people_demo_furniture_tests PRIVATE
            -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
    endif()
    add_test(NAME people_demo_furniture_tests COMMAND people_demo_furniture_tests)

    add_executable(people_resident_model_tests
        tests/ResidentModelTests.cpp
    )
    target_link_libraries(people_resident_model_tests PRIVATE people_core)
    if(MSVC)
        target_compile_options(people_resident_model_tests PRIVATE /W4 /permissive-)
    else()
        target_compile_options(people_resident_model_tests PRIVATE
            -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
    endif()
    add_test(NAME people_resident_model_tests COMMAND people_resident_model_tests)

    add_executable(people_resident_presentation_tests
        tests/ResidentPresentationTests.cpp
    )
    target_link_libraries(people_resident_presentation_tests PRIVATE people_core)
    if(MSVC)
        target_compile_options(people_resident_presentation_tests PRIVATE /W4 /permissive-)
    else()
        target_compile_options(people_resident_presentation_tests PRIVATE
            -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
    endif()
    add_test(NAME people_resident_presentation_tests COMMAND people_resident_presentation_tests)

    add_executable(people_static_navigation_tests
        tests/StaticNavigationGridTests.cpp
    )
    target_link_libraries(people_static_navigation_tests PRIVATE people_core)
    if(MSVC)
        target_compile_options(people_static_navigation_tests PRIVATE /W4 /permissive-)
    else()
        target_compile_options(people_static_navigation_tests PRIVATE
            -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
    endif()
    add_test(NAME people_static_navigation_tests COMMAND people_static_navigation_tests)

    add_test(NAME people_runtime_smoke COMMAND People --smoke-test)
    set_tests_properties(people_runtime_smoke PROPERTIES
        TIMEOUT 15
        LABELS "runtime"
    )
endif()
