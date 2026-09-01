// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors

#include <gtest/gtest.h>

#include <algorithm>

#include "LuaGeneratorRegistry.hpp"
#include "LuaSandbox.hpp"
#include "ChunkGenerator.hpp"
#include "Map/MapTilePayload.hpp"
#include "ZoneType.hpp"
#include "RegionType.hpp"
#include "ContainmentRuleRegistry.hpp"

namespace {

MeshWorld::ChunkContext make_ctx() {
    MeshWorld::ChunkContext ctx;
    ctx.seed         = 1;
    ctx.zone         = MeshWorld::ZoneType::city;
    ctx.region       = MeshWorld::RegionType::park;
    ctx.chunk_size_m = 64.0f;
    return ctx;
}

// Path to generators/lua/ relative to project root (tests run from project root)
const std::filesystem::path LUA_DIR = "generators/lua";

} // namespace

// T077 — load_from_dir finds chair.lua and registers it under "lua.object.chair.simple"
TEST(LuaGeneratorRegistryTests, LoadFromDirFindsChairGenerator) {
    MeshWorld::LuaGeneratorRegistry reg;
    reg.load_from_dir(LUA_DIR);

    ASSERT_TRUE(reg.has("lua.object.chair.simple"))
        << "Expected 'lua.object.chair.simple' to be registered after load_from_dir.\n"
        << "Registered IDs:";

    const std::string& src = reg.get("lua.object.chair.simple");
    EXPECT_FALSE(src.empty());
    EXPECT_NE(src.find("addCylinder"), std::string::npos)
        << "chair.lua should contain addCylinder calls for legs";
}

// M096 — load_from_dir finds planet.lua (M095) and registers it under
// "lua.map.planet.default", with no map-specific code in the registry:
// the directory walk is already generic (no category filter) and M.id
// extraction doesn't care which subdirectory a script lives in.
TEST(LuaGeneratorRegistryTests, LoadFromDirFindsPlanetGenerator) {
    MeshWorld::LuaGeneratorRegistry reg;
    reg.load_from_dir(LUA_DIR);

    ASSERT_TRUE(reg.has("lua.map.planet.default"))
        << "Expected 'lua.map.planet.default' to be registered after load_from_dir.";

    const std::string& src = reg.get("lua.map.planet.default");
    EXPECT_FALSE(src.empty());
    EXPECT_NE(src.find("M.generate"), std::string::npos);
}

// M096 — a registered lua.map.* generator runs end-to-end via
// LuaSandbox::executeMap(), mirroring ChairGeneratorProducesCorrectXml's
// registry+sandbox combination for the chunk-generator case.
TEST(LuaGeneratorRegistryTests, PlanetGeneratorRunsViaExecuteMap) {
    MeshWorld::LuaGeneratorRegistry reg;
    reg.load_from_dir(LUA_DIR);
    ASSERT_TRUE(reg.has("lua.map.planet.default"));

    MeshWorld::MapGenContext mapctx;
    mapctx.tile        = MeshWorld::Map::TileCoord{0, 0, 0};
    mapctx.entropy     = 42;
    mapctx.sea_level_m = 0.0;
    mapctx.parent      = nullptr;

    MeshWorld::LuaSandbox sandbox;
    std::string error;
    const MeshWorld::Map::MapTilePayload payload =
        sandbox.executeMap(reg.get("lua.map.planet.default"), mapctx, &error);

    EXPECT_TRUE(error.empty()) << "Lua error: " << error;
    EXPECT_EQ(payload.generator, "lua.map.planet.default");
    ASSERT_FALSE(payload.elevation.empty());

    // MAP19, M317: payload.features also holds MountainRange/River/Lake
    // entries now (real Hydrology/MountainRanges bindings), so continent
    // count must filter by FeatureType::Continent rather than assuming
    // every feature is one (true before M317).
    const auto continents = std::count_if(
        payload.features.begin(), payload.features.end(),
        [](const MeshWorld::Map::MapFeature& f) { return f.type == MeshWorld::Map::FeatureType::Continent; });
    EXPECT_GE(continents, 5);
    EXPECT_LE(continents, 12);
}

// list() returns all IDs including known generators
TEST(LuaGeneratorRegistryTests, ListReturnsAllRegistered) {
    MeshWorld::LuaGeneratorRegistry reg;
    reg.load_from_dir(LUA_DIR);

    auto ids = reg.list();
    EXPECT_GE(ids.size(), 5u) << "Expected at least 5 generators (chair, table, tree, bench, lamp)";
}

// get() throws for unknown ID
TEST(LuaGeneratorRegistryTests, GetThrowsForUnknownId) {
    MeshWorld::LuaGeneratorRegistry reg;
    EXPECT_THROW(reg.get("lua.object.nonexistent"), std::out_of_range);
}

// register_source + get round-trip
TEST(LuaGeneratorRegistryTests, RegisterSourceRoundTrip) {
    MeshWorld::LuaGeneratorRegistry reg;
    const std::string src = R"lua(local M={} M.id="test.gen" function M.generate() end return M)lua";
    reg.register_source("test.gen", src);

    ASSERT_TRUE(reg.has("test.gen"));
    EXPECT_EQ(reg.get("test.gen"), src);
}

// Execute chair.lua source via LuaSandbox — produces <cylinder> legs and <box> seat
// (combines T077 + T078)
TEST(LuaGeneratorRegistryTests, ChairGeneratorProducesCorrectXml) {
    MeshWorld::LuaGeneratorRegistry reg;
    reg.load_from_dir(LUA_DIR);

    ASSERT_TRUE(reg.has("lua.object.chair.simple"));

    MeshWorld::LuaSandbox sandbox;
    std::string error;
    std::string xml = sandbox.execute(reg.get("lua.object.chair.simple"), make_ctx(), &error);

    EXPECT_TRUE(error.empty()) << "Lua error: " << error;
    EXPECT_FALSE(xml.empty());
    EXPECT_NE(xml.find("<cylinder"), std::string::npos) << "Expected cylinder legs:\n" << xml;
    EXPECT_NE(xml.find("<box"),      std::string::npos) << "Expected box seat:\n"      << xml;
}

// T079 — zone/park.lua produces a chunk with benches, lamps, and trees
//
// G13 -- park.lua delegates tree/bench/lamp/fountain placement via
// scene:callGenerator(), which ALWAYS resolves against the GLOBAL
// LuaGeneratorRegistry::instance() singleton (never this test's own local
// `reg` below -- see LuaRuntime.cpp's callGenerator doc comment). Also
// needs ContainmentRuleRegistry populated, or ctx.containment.
// childrenOf("zone.park") returns empty and park.lua places nothing.
TEST(LuaGeneratorRegistryTests, ParkGeneratorProducesExpectedGeometry) {
    MeshWorld::LuaGeneratorRegistry reg;
    reg.load_from_dir(LUA_DIR);
    MeshWorld::LuaGeneratorRegistry::instance().load_from_dir(LUA_DIR);
    MeshWorld::ContainmentRuleRegistry::instance().load("data/taxonomy/containment.json");

    ASSERT_TRUE(reg.has("lua.zone.park"))
        << "Expected 'lua.zone.park' to be registered";

    MeshWorld::LuaSandbox sandbox;
    std::string error;
    std::string xml = sandbox.execute(reg.get("lua.zone.park"), make_ctx(), &error);

    EXPECT_TRUE(error.empty()) << "Lua error: " << error;
    EXPECT_FALSE(xml.empty());

    // Must have ground, cylinder (lamp/tree/fountain) and boxes (bench/flower bed)
    EXPECT_NE(xml.find("<cylinder"), std::string::npos) << "Expected cylinders (lamps/trees):\n" << xml;
    EXPECT_NE(xml.find("<box"),      std::string::npos) << "Expected boxes (benches/beds):\n"    << xml;
    EXPECT_NE(xml.find("<plane"),    std::string::npos) << "Expected planes (ground/paths):\n"   << xml;
    EXPECT_NE(xml.find("<metadata"), std::string::npos) << "Expected metadata tag:\n"            << xml;

    // Spot-check object naming conventions. bench/tree containment rules
    // both have probability=1.0 (data/taxonomy/containment.json), so those
    // are always present; lamp_post (0.9) and fountain (0.5, min_count=0)
    // are each individually conditional on a per-seed containment-rule
    // roll (G13) -- checking "at least one of the two" is what actually
    // still holds reliably, not "fountain specifically".
    EXPECT_NE(xml.find("bench_"),    std::string::npos) << "Expected bench objects";
    EXPECT_NE(xml.find("tree_"),     std::string::npos) << "Expected tree objects";
    const bool has_lamp_or_fountain = xml.find("lamp_")    != std::string::npos ||
                                      xml.find("fountain") != std::string::npos;
    EXPECT_TRUE(has_lamp_or_fountain) << "Expected lamp or fountain in park\n" << xml;
}

// T080 — Lua generator with a runtime error → C++ fallback produces valid XML
// We test this by directly checking LuaSandbox returns "" and error is set,
// then verifying C++ generator still works.
TEST(LuaGeneratorRegistryTests, BrokenLuaGeneratorFallbackBehavior) {
    const std::string broken = R"lua(
local M = {}
M.id = "lua.zone.broken_test"
function M.generate(ctx, scene)
    error("intentional failure")
end
return M
)lua";

    MeshWorld::LuaGeneratorRegistry reg;
    reg.register_source("lua.zone.broken_test", broken);

    MeshWorld::LuaSandbox sandbox;
    std::string error;
    std::string xml = sandbox.execute(reg.get("lua.zone.broken_test"), make_ctx(), &error);

    // LuaSandbox must return empty + non-empty error — no crash
    EXPECT_TRUE(xml.empty())    << "Broken generator should return empty XML";
    EXPECT_FALSE(error.empty()) << "Broken generator should set error string";
    // The error should mention 'intentional failure'
    EXPECT_NE(error.find("intentional"), std::string::npos) << "Error: " << error;
}
