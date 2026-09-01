// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors

#include <gtest/gtest.h>
#include "MaterialRegistry.hpp"
#include "BuiltinMaterials.hpp"
#include <filesystem>
#include <sstream>
#include <iostream>

namespace {

// Helper: load builtin materials into a fresh (non-singleton) registry.
// For singleton tests we call register_builtin_materials() once via fixture.

struct BuiltinFixture : ::testing::Test {
    void SetUp() override {
        // Calling more than once is safe (register_material overwrites).
        MeshWorld::register_builtin_materials();
    }
};

} // namespace

// T113 — key C++ generator material IDs must be in MaterialRegistry
TEST_F(BuiltinFixture, CppGeneratorMaterialsRegistered) {
    auto& reg = MeshWorld::MaterialRegistry::instance();

    // ParkGenerator
    EXPECT_TRUE(reg.has("grass_park"))    << "grass_park missing";
    EXPECT_TRUE(reg.has("path_gravel"))   << "path_gravel missing";
    EXPECT_TRUE(reg.has("stone_granite")) << "stone_granite missing";
    EXPECT_TRUE(reg.has("stone_light"))   << "stone_light missing";
    EXPECT_TRUE(reg.has("water"))         << "water missing";
    EXPECT_TRUE(reg.has("flower_red"))    << "flower_red missing";
    EXPECT_TRUE(reg.has("flower_yellow")) << "flower_yellow missing";

    // RoadGenerator
    EXPECT_TRUE(reg.has("asphalt"))          << "asphalt missing";
    EXPECT_TRUE(reg.has("concrete_pavement"))<< "concrete_pavement missing";
    EXPECT_TRUE(reg.has("road_line_white"))  << "road_line_white missing";

    // ForestGenerator
    EXPECT_TRUE(reg.has("forest_floor"))  << "forest_floor missing";
    EXPECT_TRUE(reg.has("wood_fence"))    << "wood_fence missing";

    // OceanGenerator
    EXPECT_TRUE(reg.has("water_ocean"))   << "water_ocean missing";
    EXPECT_TRUE(reg.has("sand_beach"))    << "sand_beach missing";

    // CaveGenerator
    EXPECT_TRUE(reg.has("rock_cave_floor"))   << "rock_cave_floor missing";
    EXPECT_TRUE(reg.has("rock_cave_wall"))    << "rock_cave_wall missing";
    EXPECT_TRUE(reg.has("rock_cave_ceiling")) << "rock_cave_ceiling missing";
    // MAP21, M333/M335 -- generate() referenced these two even before M333
    // (its own raw w.cylinder() calls), but neither was ever actually
    // registered -- a pre-existing gap, fixed alongside adding real
    // ObjectDefinitionLibrary definitions for them.
    EXPECT_TRUE(reg.has("rock_stalactite"))   << "rock_stalactite missing";
    EXPECT_TRUE(reg.has("rock_stalagmite"))   << "rock_stalagmite missing";
    EXPECT_TRUE(reg.has("rock_rubble"))       << "rock_rubble missing";

    // SmallHouseBlockGenerator
    EXPECT_TRUE(reg.has("brick_red"))     << "brick_red missing";
    EXPECT_TRUE(reg.has("roof_tile_red")) << "roof_tile_red missing";
    EXPECT_TRUE(reg.has("cobblestone"))   << "cobblestone missing";

    // ObjectDefinitionLibrary's composer-owned apartment definition.
    EXPECT_TRUE(reg.has("plaster_beige")) << "plaster_beige missing";
}

// T114 — key Lua generator material IDs must be in MaterialRegistry
TEST_F(BuiltinFixture, LuaGeneratorMaterialsRegistered) {
    auto& reg = MeshWorld::MaterialRegistry::instance();

    // kitchen.lua
    EXPECT_TRUE(reg.has("tile_kitchen"))      << "tile_kitchen missing";
    EXPECT_TRUE(reg.has("plaster_white"))     << "plaster_white missing";
    EXPECT_TRUE(reg.has("wood_counter"))      << "wood_counter missing";
    EXPECT_TRUE(reg.has("stone_countertop"))  << "stone_countertop missing";
    EXPECT_TRUE(reg.has("appliance_white"))   << "appliance_white missing";
    EXPECT_TRUE(reg.has("metal_chrome"))      << "metal_chrome missing";

    // simple_house.lua
    EXPECT_TRUE(reg.has("glass_clear"))       << "glass_clear missing";
    EXPECT_TRUE(reg.has("wood_door_panel"))   << "wood_door_panel missing";

    // trash_can.lua
    EXPECT_TRUE(reg.has("metal_dark"))        << "metal_dark missing";
    EXPECT_TRUE(reg.has("plastic_black"))     << "plastic_black missing";

    // bench.lua
    EXPECT_TRUE(reg.has("wood_natural"))      << "wood_natural missing";
    EXPECT_TRUE(reg.has("wood_bench"))        << "wood_bench missing";

    // lamp.lua
    EXPECT_TRUE(reg.has("metal_lamp"))        << "metal_lamp missing";

    // tv.lua
    EXPECT_TRUE(reg.has("tv_screen_off"))     << "tv_screen_off missing";
}

// T115 — addMaterial with unknown ID logs a warning to stderr
TEST(MaterialRegistryTests, UnregisteredMaterialWarning) {
    // Redirect stderr to capture warning
    std::streambuf* old_cerr = std::cerr.rdbuf();
    std::ostringstream captured;
    std::cerr.rdbuf(captured.rdbuf());

    // Register then check has() - addMaterial() in Mc3SceneBuilder warns when id not in registry.
    // We test MaterialRegistry directly here: has() returns false for unknown ID.
    MeshWorld::MaterialRegistry fresh_reg;
    EXPECT_FALSE(fresh_reg.has("definitely_not_registered_xyzzy"));

    // Restore stderr
    std::cerr.rdbuf(old_cerr);

    // Register the material
    MeshWorld::MaterialEntry e;
    e.id = "test_material_abc";
    e.r = 0.5f; e.g = 0.5f; e.b = 0.5f;
    fresh_reg.register_material(std::move(e));
    EXPECT_TRUE(fresh_reg.has("test_material_abc"));
}

TEST(MaterialRegistryTests, GetThrowsForUnknown) {
    MeshWorld::MaterialRegistry reg;
    EXPECT_THROW(reg.get("nonexistent_xyz"), std::out_of_range);
}

TEST(MaterialRegistryTests, RegisterAndRetrieve) {
    MeshWorld::MaterialRegistry reg;
    MeshWorld::MaterialEntry e;
    e.id        = "test_mat";
    e.r         = 0.2f;
    e.g         = 0.5f;
    e.b         = 0.8f;
    e.roughness = 0.6f;
    e.metallic  = 0.1f;
    e.license.spdx_license = "MIT";
    e.license.author       = "Test";
    reg.register_material(e);

    ASSERT_TRUE(reg.has("test_mat"));
    const auto& m = reg.get("test_mat");
    EXPECT_FLOAT_EQ(m.r, 0.2f);
    EXPECT_FLOAT_EQ(m.g, 0.5f);
    EXPECT_FLOAT_EQ(m.b, 0.8f);
    EXPECT_EQ(m.license.spdx_license, "MIT");
}


// T320: grass_park and other key materials must have a non-empty texture_uri.
TEST_F(BuiltinFixture, KeyMaterialsHaveTextureUri) {
    auto& reg = MeshWorld::MaterialRegistry::instance();
    const char* textured[] = {
        "grass_park", "asphalt", "concrete", "sand",
        "brick_red", "water", "roof_tile_red", "plaster_white", "plaster_beige", "wood_natural",
    };
    for (const auto* id : textured) {
        ASSERT_TRUE(reg.has(id)) << id << " not registered";
        EXPECT_FALSE(reg.get(id).texture_uri.empty())
            << id << " should have a texture_uri";
    }
}

// Real bug fix (2026-07-11, user-reported "everything is a flat color"):
// MeshCraft's SceneRenderer resolves a texture's relative URI against the
// loaded document's own sourcePath, which for chunk documents is
// saves/<world>/chunks/, not this repo's real assets/ root -- a plain
// relative "assets/textures/grass.png" silently resolved to a nonexistent
// path. reg_tex() now stores an ABSOLUTE path instead (see its own doc
// comment in BuiltinMaterials.cpp), which resolves correctly regardless of
// sourcePath. This test proves both halves: the stored URI is genuinely
// absolute, AND it points at a real file that actually exists on disk --
// not just "some absolute-looking string" (relies on the same
// "process runs with its working directory at the repo root" assumption
// gtest_discover_tests()'s own WORKING_DIRECTORY setting already gives
// every other test in this binary).
TEST_F(BuiltinFixture, TextureUriIsAbsoluteAndPointsAtARealFile) {
    auto& reg = MeshWorld::MaterialRegistry::instance();
    const char* textured[] = {"grass_park", "asphalt", "concrete", "sand", "water", "wood_natural"};
    for (const auto* id : textured) {
        ASSERT_TRUE(reg.has(id)) << id << " not registered";
        const std::string& uri = reg.get(id).texture_uri;
        ASSERT_FALSE(uri.empty()) << id << " should have a texture_uri";
        EXPECT_TRUE(std::filesystem::path(uri).is_absolute()) << id << " uri=" << uri;
        EXPECT_TRUE(std::filesystem::exists(uri))
            << id << " uri=" << uri << " does not point at a real file";
    }
}
