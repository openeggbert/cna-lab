cmake_minimum_required(VERSION 3.20)

project(wolf_cna LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CNA_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../cna"
    CACHE PATH "Path to the CNA checkout")

if(NOT EXISTS "${CNA_ROOT}/CMakeLists.txt")
    message(FATAL_ERROR
        "wolf-cna: CNA checkout not found at '${CNA_ROOT}'. "
        "Set -DCNA_ROOT=/path/to/cna.")
endif()

if(NOT DEFINED CNA_GRAPHICS_RENDERER)
    if(EMSCRIPTEN)
        set(CNA_GRAPHICS_RENDERER "WEBGL2" CACHE STRING
            "CNA renderer used by wolf-cna")
    else()
        set(CNA_GRAPHICS_RENDERER "OPENGLES3" CACHE STRING
            "CNA renderer used by wolf-cna")
    endif()
endif()

set(CNA_BUILD_TESTS OFF CACHE BOOL "Build CNA tests" FORCE)
set(CNA_BUILD_EXAMPLES OFF CACHE BOOL "Build CNA examples" FORCE)
set(CNA_ENABLE_NET OFF CACHE BOOL "Build CNA networking" FORCE)
set(CNA_ENABLE_VIDEO OFF CACHE STRING "Enable CNA video" FORCE)
set(SHARP_RUNTIME_BUILD_TESTS OFF CACHE BOOL "Build sharp-runtime tests" FORCE)
set(EASYGL_BUILD_TESTS OFF CACHE BOOL "Build EasyGL tests" FORCE)
set(EASYGL_BUILD_EXAMPLES OFF CACHE BOOL "Build EasyGL examples" FORCE)

add_subdirectory("${CNA_ROOT}" "${CMAKE_BINARY_DIR}/cna-build")

add_executable(wolf-cna
    src/CampaignProgress.cpp
    src/Combat.cpp
    src/DoorMotion.cpp
    src/ExplorationMap.cpp
    src/LevelDefinition.cpp
    src/Program.cpp
    src/RunSave.cpp
    src/SpatialAudio.cpp
    src/WolfGame.cpp
    src/World.cpp
)

target_include_directories(wolf-cna PRIVATE src)
target_link_libraries(wolf-cna PRIVATE CNA)

configure_file(
    assets/levels/starter.level
    "${CMAKE_CURRENT_BINARY_DIR}/assets/levels/starter.level"
    COPYONLY)
configure_file(
    assets/levels/sector-02.level
    "${CMAKE_CURRENT_BINARY_DIR}/assets/levels/sector-02.level"
    COPYONLY)
configure_file(
    assets/levels/sector-03.level
    "${CMAKE_CURRENT_BINARY_DIR}/assets/levels/sector-03.level"
    COPYONLY)
configure_file(
    assets/levels/sector-04.level
    "${CMAKE_CURRENT_BINARY_DIR}/assets/levels/sector-04.level"
    COPYONLY)
configure_file(
    assets/levels/hidden-reservoir.level
    "${CMAKE_CURRENT_BINARY_DIR}/assets/levels/hidden-reservoir.level"
    COPYONLY)
configure_file(
    assets/levels/warden-core.level
    "${CMAKE_CURRENT_BINARY_DIR}/assets/levels/warden-core.level"
    COPYONLY)
configure_file(
    assets/sprites/security-guard.png
    "${CMAKE_CURRENT_BINARY_DIR}/assets/sprites/security-guard.png"
    COPYONLY)
configure_file(
    assets/sprites/security-hound.png
    "${CMAKE_CURRENT_BINARY_DIR}/assets/sprites/security-hound.png"
    COPYONLY)
configure_file(
    assets/sprites/rapid-trooper.png
    "${CMAKE_CURRENT_BINARY_DIR}/assets/sprites/rapid-trooper.png"
    COPYONLY)
configure_file(
    assets/sprites/heavy-unit.png
    "${CMAKE_CURRENT_BINARY_DIR}/assets/sprites/heavy-unit.png"
    COPYONLY)
configure_file(
    assets/sprites/bunker-warden.png
    "${CMAKE_CURRENT_BINARY_DIR}/assets/sprites/bunker-warden.png"
    COPYONLY)
configure_file(
    assets/sprites/security-guard-attack.png
    "${CMAKE_CURRENT_BINARY_DIR}/assets/sprites/security-guard-attack.png"
    COPYONLY)
configure_file(
    assets/sprites/security-hound-attack.png
    "${CMAKE_CURRENT_BINARY_DIR}/assets/sprites/security-hound-attack.png"
    COPYONLY)
configure_file(
    assets/sprites/rapid-trooper-attack.png
    "${CMAKE_CURRENT_BINARY_DIR}/assets/sprites/rapid-trooper-attack.png"
    COPYONLY)
configure_file(
    assets/sprites/heavy-unit-attack.png
    "${CMAKE_CURRENT_BINARY_DIR}/assets/sprites/heavy-unit-attack.png"
    COPYONLY)
configure_file(
    assets/sprites/bunker-warden-attack.png
    "${CMAKE_CURRENT_BINARY_DIR}/assets/sprites/bunker-warden-attack.png"
    COPYONLY)
configure_file(
    assets/sprites/security-guard-pain.png
    "${CMAKE_CURRENT_BINARY_DIR}/assets/sprites/security-guard-pain.png"
    COPYONLY)
configure_file(
    assets/sprites/security-hound-pain.png
    "${CMAKE_CURRENT_BINARY_DIR}/assets/sprites/security-hound-pain.png"
    COPYONLY)
configure_file(
    assets/sprites/rapid-trooper-pain.png
    "${CMAKE_CURRENT_BINARY_DIR}/assets/sprites/rapid-trooper-pain.png"
    COPYONLY)
configure_file(
    assets/sprites/heavy-unit-pain.png
    "${CMAKE_CURRENT_BINARY_DIR}/assets/sprites/heavy-unit-pain.png"
    COPYONLY)
configure_file(
    assets/sprites/bunker-warden-pain.png
    "${CMAKE_CURRENT_BINARY_DIR}/assets/sprites/bunker-warden-pain.png"
    COPYONLY)
configure_file(
    assets/sprites/security-guard-defeated.png
    "${CMAKE_CURRENT_BINARY_DIR}/assets/sprites/security-guard-defeated.png"
    COPYONLY)
configure_file(
    assets/sprites/security-hound-defeated.png
    "${CMAKE_CURRENT_BINARY_DIR}/assets/sprites/security-hound-defeated.png"
    COPYONLY)
configure_file(
    assets/sprites/rapid-trooper-defeated.png
    "${CMAKE_CURRENT_BINARY_DIR}/assets/sprites/rapid-trooper-defeated.png"
    COPYONLY)
configure_file(
    assets/sprites/heavy-unit-defeated.png
    "${CMAKE_CURRENT_BINARY_DIR}/assets/sprites/heavy-unit-defeated.png"
    COPYONLY)
configure_file(
    assets/sprites/bunker-warden-defeated.png
    "${CMAKE_CURRENT_BINARY_DIR}/assets/sprites/bunker-warden-defeated.png"
    COPYONLY)
configure_file(
    assets/weapons/knife.png
    "${CMAKE_CURRENT_BINARY_DIR}/assets/weapons/knife.png"
    COPYONLY)
configure_file(
    assets/weapons/sidearm.png
    "${CMAKE_CURRENT_BINARY_DIR}/assets/weapons/sidearm.png"
    COPYONLY)
configure_file(
    assets/weapons/repeater.png
    "${CMAKE_CURRENT_BINARY_DIR}/assets/weapons/repeater.png"
    COPYONLY)
configure_file(
    assets/weapons/heavy-automatic.png
    "${CMAKE_CURRENT_BINARY_DIR}/assets/weapons/heavy-automatic.png"
    COPYONLY)
configure_file(
    assets/weapons/knife-attack.png
    "${CMAKE_CURRENT_BINARY_DIR}/assets/weapons/knife-attack.png"
    COPYONLY)
configure_file(
    assets/weapons/sidearm-attack.png
    "${CMAKE_CURRENT_BINARY_DIR}/assets/weapons/sidearm-attack.png"
    COPYONLY)
configure_file(
    assets/weapons/repeater-attack.png
    "${CMAKE_CURRENT_BINARY_DIR}/assets/weapons/repeater-attack.png"
    COPYONLY)
configure_file(
    assets/weapons/heavy-automatic-attack.png
    "${CMAKE_CURRENT_BINARY_DIR}/assets/weapons/heavy-automatic-attack.png"
    COPYONLY)
configure_file(
    assets/pickups/ammo-box.png
    "${CMAKE_CURRENT_BINARY_DIR}/assets/pickups/ammo-box.png"
    COPYONLY)
configure_file(
    assets/pickups/health-kit.png
    "${CMAKE_CURRENT_BINARY_DIR}/assets/pickups/health-kit.png"
    COPYONLY)
configure_file(
    assets/pickups/field-dressing.png
    "${CMAKE_CURRENT_BINARY_DIR}/assets/pickups/field-dressing.png"
    COPYONLY)
configure_file(
    assets/pickups/gold-bars.png
    "${CMAKE_CURRENT_BINARY_DIR}/assets/pickups/gold-bars.png"
    COPYONLY)
configure_file(
    assets/pickups/golden-goblet.png
    "${CMAKE_CURRENT_BINARY_DIR}/assets/pickups/golden-goblet.png"
    COPYONLY)
configure_file(
    assets/pickups/peace-medallion.png
    "${CMAKE_CURRENT_BINARY_DIR}/assets/pickups/peace-medallion.png"
    COPYONLY)
configure_file(
    assets/pickups/peace-prism.png
    "${CMAKE_CURRENT_BINARY_DIR}/assets/pickups/peace-prism.png"
    COPYONLY)
configure_file(
    assets/pickups/amber-access-card.png
    "${CMAKE_CURRENT_BINARY_DIR}/assets/pickups/amber-access-card.png"
    COPYONLY)
configure_file(
    assets/pickups/recovery-beacon.png
    "${CMAKE_CURRENT_BINARY_DIR}/assets/pickups/recovery-beacon.png"
    COPYONLY)
configure_file(
    assets/materials/wall-stone.png
    "${CMAKE_CURRENT_BINARY_DIR}/assets/materials/wall-stone.png"
    COPYONLY)
configure_file(
    assets/materials/wall-brick.png
    "${CMAKE_CURRENT_BINARY_DIR}/assets/materials/wall-brick.png"
    COPYONLY)
configure_file(
    assets/materials/wall-steel.png
    "${CMAKE_CURRENT_BINARY_DIR}/assets/materials/wall-steel.png"
    COPYONLY)
configure_file(
    assets/materials/wall-lab.png
    "${CMAKE_CURRENT_BINARY_DIR}/assets/materials/wall-lab.png"
    COPYONLY)
configure_file(
    assets/materials/table-wood.png
    "${CMAKE_CURRENT_BINARY_DIR}/assets/materials/table-wood.png"
    COPYONLY)
configure_file(
    assets/props/access-card.png
    "${CMAKE_CURRENT_BINARY_DIR}/assets/props/access-card.png"
    COPYONLY)
configure_file(
    assets/props/repeater-pickup.png
    "${CMAKE_CURRENT_BINARY_DIR}/assets/props/repeater-pickup.png"
    COPYONLY)
configure_file(
    assets/props/heavy-automatic-pickup.png
    "${CMAKE_CURRENT_BINARY_DIR}/assets/props/heavy-automatic-pickup.png"
    COPYONLY)
configure_file(
    assets/props/terminal.png
    "${CMAKE_CURRENT_BINARY_DIR}/assets/props/terminal.png"
    COPYONLY)
configure_file(
    assets/props/power-relay.png
    "${CMAKE_CURRENT_BINARY_DIR}/assets/props/power-relay.png"
    COPYONLY)
configure_file(
    assets/props/sector-exit.png
    "${CMAKE_CURRENT_BINARY_DIR}/assets/props/sector-exit.png"
    COPYONLY)
configure_file(
    assets/props/enemy-projectile.png
    "${CMAKE_CURRENT_BINARY_DIR}/assets/props/enemy-projectile.png"
    COPYONLY)
configure_file(
    assets/decorations/storage-plant.png
    "${CMAKE_CURRENT_BINARY_DIR}/assets/decorations/storage-plant.png"
    COPYONLY)
configure_file(
    assets/decorations/foundry-plant.png
    "${CMAKE_CURRENT_BINARY_DIR}/assets/decorations/foundry-plant.png"
    COPYONLY)
configure_file(
    assets/decorations/labs-plant.png
    "${CMAKE_CURRENT_BINARY_DIR}/assets/decorations/labs-plant.png"
    COPYONLY)
configure_file(
    assets/decorations/archive-plant.png
    "${CMAKE_CURRENT_BINARY_DIR}/assets/decorations/archive-plant.png"
    COPYONLY)
configure_file(
    assets/title/title-background.png
    "${CMAKE_CURRENT_BINARY_DIR}/assets/title/title-background.png"
    COPYONLY)

enable_testing()
add_executable(level-definition-tests
    src/CampaignProgress.cpp
    src/Combat.cpp
    src/DoorMotion.cpp
    src/ExplorationMap.cpp
    src/LevelDefinition.cpp
    src/RunSave.cpp
    src/SpatialAudio.cpp
    src/World.cpp
    tests/LevelDefinitionTests.cpp
)
target_include_directories(level-definition-tests PRIVATE src)
target_link_libraries(level-definition-tests PRIVATE CNA)
add_test(NAME level-definition-tests COMMAND level-definition-tests)
set_tests_properties(level-definition-tests PROPERTIES
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")

if(TARGET SDL3::SDL3main)
    target_link_libraries(wolf-cna PRIVATE SDL3::SDL3main)
endif()

if(WIN32)
    set_target_properties(wolf-cna PROPERTIES WIN32_EXECUTABLE TRUE)
    if(COMMAND cna_copy_sdl_runtime)
        cna_copy_sdl_runtime(wolf-cna)
    endif()
endif()
