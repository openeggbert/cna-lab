// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
//
// M090/M091 — LuaRuntime's map-generator mode: binds MapBuilder as the Lua
// global `map` (table-arg API mirroring register_scene_api()'s style for
// chunk generators) and a `names` table (M091, stub phonotactic naming). No
// sandboxed execute()/ctx fields yet (M092-M094) — this tests the binding
// itself: a Lua script can drive MapBuilder/names and the resulting
// Map::MapTilePayload reflects it.

#include <gtest/gtest.h>

#include "LuaRuntime.hpp"
#include "MapBuilder.hpp"
#include "ZoneType.hpp"

using namespace MeshWorld;
using namespace MeshWorld::Map;

namespace {

MapBuilder make_builder() {
    return MapBuilder(TileCoord{0, 0, 0}, 42, /*sea_level_m=*/0.0);
}

} // namespace

TEST(LuaRuntimeMapBindingTest, SetBiomeFieldFromLua) {
    MapBuilder builder = make_builder();
    LuaRuntime runtime(builder);

    constexpr const char* SOURCE = R"lua(
local M = {}
function M.generate(ctx, map)
    map:setBiomeField(2, 1, {-10.0, 100.0}, {20.0, 20.0}, {0.5, 0.5})
end
return M
)lua";

    const std::string err = runtime.run(SOURCE);
    EXPECT_EQ(err, "");

    const auto& p = builder.payload();
    ASSERT_EQ(p.elevation.w, 2);
    ASSERT_EQ(p.elevation.h, 1);
    EXPECT_FLOAT_EQ(p.elevation.data[0], -10.0f);
    EXPECT_FLOAT_EQ(p.elevation.data[1], 100.0f);
    ASSERT_FALSE(p.biome.empty());
    // depth=10m, shallow, moderate temp -> kelp_forest specifically since
    // M236-M275's ocean-family subtyping (MAP16, 2026-07-10).
    EXPECT_EQ(static_cast<ZoneType>(p.biome.data[0]), ZoneType::kelp_forest);
}

// M095 — addContinent (added for the Lua planet.lua port).
TEST(LuaRuntimeMapBindingTest, AddContinentFromLua) {
    MapBuilder builder = make_builder();
    LuaRuntime runtime(builder);

    constexpr const char* SOURCE = R"lua(
local M = {}
function M.generate(ctx, map)
    map:addContinent("Vorlandia", 100.0, 200.0)
end
return M
)lua";

    EXPECT_EQ(runtime.run(SOURCE), "");

    ASSERT_EQ(builder.payload().features.size(), 1u);
    const auto& f = builder.payload().features[0];
    EXPECT_EQ(f.type, FeatureType::Continent);
    EXPECT_EQ(f.name, "Vorlandia");
    ASSERT_EQ(f.points.size(), 1u);
    EXPECT_EQ(f.points[0][0], 100.0);
    EXPECT_EQ(f.points[0][1], 200.0);
}

TEST(LuaRuntimeMapBindingTest, AddRiverFromLua) {
    MapBuilder builder = make_builder();
    LuaRuntime runtime(builder);

    constexpr const char* SOURCE = R"lua(
local M = {}
function M.generate(ctx, map)
    map:addRiver("Skarnfoss", { {0, 0}, {1, 2}, {3, 4} })
end
return M
)lua";

    EXPECT_EQ(runtime.run(SOURCE), "");

    ASSERT_EQ(builder.payload().features.size(), 1u);
    const auto& f = builder.payload().features[0];
    EXPECT_EQ(f.type, FeatureType::River);
    EXPECT_EQ(f.name, "Skarnfoss");
    ASSERT_EQ(f.points.size(), 3u);
    EXPECT_EQ(f.points[2][0], 3.0);
    EXPECT_EQ(f.points[2][1], 4.0);
}

TEST(LuaRuntimeMapBindingTest, AddRoadFromLua) {
    MapBuilder builder = make_builder();
    LuaRuntime runtime(builder);

    constexpr const char* SOURCE = R"lua(
local M = {}
function M.generate(ctx, map)
    map:addRoad("Kings Way", { {0, 0}, {5, 5} })
end
return M
)lua";

    EXPECT_EQ(runtime.run(SOURCE), "");

    ASSERT_EQ(builder.payload().features.size(), 1u);
    const auto& f = builder.payload().features[0];
    EXPECT_EQ(f.type, FeatureType::Road);
    EXPECT_EQ(f.name, "Kings Way");
    ASSERT_EQ(f.points.size(), 2u);
    EXPECT_EQ(f.points[1][0], 5.0);
    EXPECT_EQ(f.points[1][1], 5.0);
}

TEST(LuaRuntimeMapBindingTest, AddCityWithAndWithoutSizeHintFromLua) {
    MapBuilder builder = make_builder();
    LuaRuntime runtime(builder);

    constexpr const char* SOURCE = R"lua(
local M = {}
function M.generate(ctx, map)
    map:addCity("Vorhavn", 12.5, 34.5)
    map:addCity("Little Elm", 1.0, 2.0, "town")
end
return M
)lua";

    EXPECT_EQ(runtime.run(SOURCE), "");

    ASSERT_EQ(builder.payload().features.size(), 2u);
    EXPECT_EQ(builder.payload().features[0].type, FeatureType::City);
    EXPECT_EQ(builder.payload().features[1].type, FeatureType::Town);
}

TEST(LuaRuntimeMapBindingTest, SetEdgeAndSetMetadataFromLua) {
    MapBuilder builder = make_builder();
    LuaRuntime runtime(builder);

    constexpr const char* SOURCE = R"lua(
local M = {}
function M.generate(ctx, map)
    map:setEdge("N", {1.0, 2.0, 3.0})
    map:setMetadata("lua.map.planet.default", "nordic")
end
return M
)lua";

    EXPECT_EQ(runtime.run(SOURCE), "");

    const std::vector<float> expected = {1.0f, 2.0f, 3.0f};
    EXPECT_EQ(builder.payload().edges[0].elevation, expected);
    EXPECT_EQ(builder.payload().generator, "lua.map.planet.default");
    EXPECT_EQ(builder.payload().culture, "nordic");
}

TEST(LuaRuntimeMapBindingTest, SandboxBlocksUnsafeGlobalsInMapMode) {
    MapBuilder builder = make_builder();
    LuaRuntime runtime(builder);

    constexpr const char* SOURCE = R"lua(
local M = {}
function M.generate(ctx, map)
    io.open("/etc/passwd", "r")
end
return M
)lua";

    const std::string err = runtime.run(SOURCE);
    EXPECT_NE(err, "");  // io is nil -> calling io.open errors, not a crash
}

TEST(LuaRuntimeMapBindingTest, MissingGenerateFunctionReportsError) {
    MapBuilder builder = make_builder();
    LuaRuntime runtime(builder);

    const std::string err = runtime.run("return {}");
    EXPECT_NE(err.find("generate"), std::string::npos);
}

// M091 — `names.*` global exposed alongside `map` in map-generator mode.
// setMetadata(generator_id, culture) is (ab)used here purely as a channel to
// smuggle the generated names back out to the C++ side for assertions.
TEST(LuaRuntimeMapBindingTest, NamesTableGeneratesNamesFromLua) {
    MapBuilder builder = make_builder();
    LuaRuntime runtime(builder);

    constexpr const char* SOURCE = R"lua(
local M = {}
function M.generate(ctx, map)
    local culture = names.culture(7)
    local city = names.city(culture, 1)
    local river = names.river(culture, 2)
    local street = names.street(culture, 3)
    map:setMetadata(city .. "|" .. river .. "|" .. street, culture)
end
return M
)lua";

    EXPECT_EQ(runtime.run(SOURCE), "");

    const auto& p = builder.payload();
    EXPECT_TRUE(p.culture == "nordic" || p.culture == "romance" || p.culture == "desert") << p.culture;

    const auto sep1 = p.generator.find('|');
    ASSERT_NE(sep1, std::string::npos);
    const auto sep2 = p.generator.find('|', sep1 + 1);
    ASSERT_NE(sep2, std::string::npos);
    const std::string city   = p.generator.substr(0, sep1);
    const std::string river  = p.generator.substr(sep1 + 1, sep2 - sep1 - 1);
    const std::string street = p.generator.substr(sep2 + 1);
    EXPECT_FALSE(city.empty());
    EXPECT_FALSE(river.empty());
    EXPECT_NE(street.find(' '), std::string::npos);  // "<adjective> <noun> <suffix>"
}

TEST(LuaRuntimeMapBindingTest, NamesCultureIsDeterministicFromLua) {
    MapBuilder builder = make_builder();
    LuaRuntime runtime(builder);

    constexpr const char* SOURCE = R"lua(
local M = {}
function M.generate(ctx, map)
    local a = names.culture(99)
    local b = names.culture(99)
    map:setMetadata("cmp", (a == b) and a or "MISMATCH")
end
return M
)lua";

    EXPECT_EQ(runtime.run(SOURCE), "");
    EXPECT_NE(builder.payload().culture, "MISMATCH");
}
