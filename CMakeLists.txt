cmake_minimum_required(VERSION 3.25...3.31)
project(BlackPine VERSION 0.1.0 LANGUAGES CXX)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
    message(FATAL_ERROR "In-source builds are unsupported; use cmake -S . -B build")
endif()

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(EXPLORE2D_SOURCE_DIR "" CACHE PATH "Path to the Explore2D source project")
if(EXPLORE2D_SOURCE_DIR STREQUAL "")
    get_filename_component(_default_explore2d "${CMAKE_CURRENT_SOURCE_DIR}/../explore-2d" ABSOLUTE)
    set(EXPLORE2D_SOURCE_DIR "${_default_explore2d}" CACHE PATH "Path to the Explore2D source project" FORCE)
endif()

if(NOT EXISTS "${EXPLORE2D_SOURCE_DIR}/CMakeLists.txt")
    message(FATAL_ERROR "EXPLORE2D_SOURCE_DIR does not contain Explore2D: ${EXPLORE2D_SOURCE_DIR}")
endif()

set(EXPLORE2D_BUILD_TESTS OFF CACHE BOOL "Tests are owned by this top-level build" FORCE)
add_subdirectory("${EXPLORE2D_SOURCE_DIR}" "${CMAKE_BINARY_DIR}/_deps/explore2d")

add_library(black_pine_world src/BlackPineWorld.cpp)
target_include_directories(black_pine_world PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(black_pine_world PUBLIC Explore2D::Core)
target_compile_features(black_pine_world PUBLIC cxx_std_23)

if(TARGET Explore2D::Cna)
    add_executable(black_pine src/Main.cpp)
    target_link_libraries(black_pine PRIVATE black_pine_world Explore2D::Cna)
    set_target_properties(black_pine PROPERTIES OUTPUT_NAME black-pine)
else()
    message(STATUS "CNA unavailable: black-pine executable is not generated")
endif()

option(BLACK_PINE_BUILD_TESTS "Build Black Pine scenario tests" ON)
if(BLACK_PINE_BUILD_TESTS)
    enable_testing()
    add_executable(black_pine_scenario_tests test/ScenarioTests.cpp)
    target_link_libraries(black_pine_scenario_tests PRIVATE black_pine_world)
    add_test(NAME black-pine.scenario COMMAND black_pine_scenario_tests)
endif()
