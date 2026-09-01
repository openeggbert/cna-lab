// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
//
// M308 (MAP18) — structural tests for generators/lua/map/level2.lua
// (level 2), mirroring CountryLuaTests.cpp's style (child-tile path,
// parent required).

#include <gtest/gtest.h>

#include <cstdint>
#include <fstream>
#include <sstream>

#include "LuaSandbox.hpp"
#include "Map/MapTilePayload.hpp"
#include "ZoneType.hpp"

using namespace MeshWorld;
using namespace MeshWorld::Map;

namespace {

constexpr int N = 64;

std::string read_level2_lua() {
    std::ifstream ifs("generators/lua/map/level2.lua");
    std::ostringstream ss;
    ss << ifs.rdbuf();
    return ss.str();
}

MapTilePayload make_moderate_land_parent(std::uint64_t entropy) {
    MapTilePayload p;
    p.tile    = TileCoord{1, 0, 0};
    p.entropy = entropy;
    p.culture = "nordic";

    p.elevation.w = p.elevation.h = N;
    p.elevation.data.assign(static_cast<std::size_t>(N * N), 1000.0f);
    p.temperature.w = p.temperature.h = N;
    p.temperature.data.assign(static_cast<std::size_t>(N * N), 15.0f);
    p.moisture.w = p.moisture.h = N;
    p.moisture.data.assign(static_cast<std::size_t>(N * N), 0.4f);
    p.biome.w = p.biome.h = N;
    p.biome.data.assign(static_cast<std::size_t>(N * N), 0);
    for (auto& e : p.edges) e.elevation.assign(static_cast<std::size_t>(N), 1000.0f);
    return p;
}

MapTilePayload make_all_ocean_parent(std::uint64_t entropy) {
    MapTilePayload p;
    p.tile    = TileCoord{1, 0, 0};
    p.entropy = entropy;
    p.culture = "nordic";

    p.elevation.w = p.elevation.h = N;
    p.elevation.data.assign(static_cast<std::size_t>(N * N), -2000.0f);
    p.temperature.w = p.temperature.h = N;
    p.temperature.data.assign(static_cast<std::size_t>(N * N), 15.0f);
    p.moisture.w = p.moisture.h = N;
    p.moisture.data.assign(static_cast<std::size_t>(N * N), 0.4f);
    p.biome.w = p.biome.h = N;
    p.biome.data.assign(static_cast<std::size_t>(N * N), 0);
    for (auto& e : p.edges) e.elevation.assign(static_cast<std::size_t>(N), -2000.0f);
    return p;
}

MapGenContext make_ctx(const MapTilePayload& parent, std::uint64_t entropy, const TileCoord& tile) {
    MapGenContext ctx;
    ctx.tile        = tile;
    ctx.entropy     = entropy;
    ctx.sea_level_m = 0.0;
    ctx.parent      = &parent;
    return ctx;
}

MapTilePayload run_level2(const MapTilePayload& parent, std::uint64_t entropy,
                                  const TileCoord& tile, std::string* error_out = nullptr) {
    LuaSandbox sandbox;
    return sandbox.executeMap(read_level2_lua(), make_ctx(parent, entropy, tile), error_out);
}

int count_feature(const MapTilePayload& payload, FeatureType type) {
    int n = 0;
    for (const auto& f : payload.features)
        if (f.type == type) ++n;
    return n;
}

} // namespace

TEST(Level2LuaTest, FileReadsAndRunsWithoutError) {
    const std::string source = read_level2_lua();
    ASSERT_FALSE(source.empty()) << "generators/lua/map/level2.lua not found or empty";

    const MapTilePayload parent = make_moderate_land_parent(1);
    std::string error;
    const MapTilePayload payload = run_level2(parent, 2, TileCoord{2, 1, 0}, &error);

    EXPECT_TRUE(error.empty()) << error;
    EXPECT_FALSE(payload.elevation.empty());
    EXPECT_EQ(payload.generator, "lua.map.child.level2.default");
    EXPECT_EQ(payload.culture, "nordic");
}

TEST(Level2LuaTest, FieldShapesMatch64x64) {
    const MapTilePayload parent  = make_moderate_land_parent(1);
    const MapTilePayload payload = run_level2(parent, 2, TileCoord{2, 1, 0});

    EXPECT_EQ(payload.elevation.w, N);
    EXPECT_EQ(payload.elevation.h, N);
    EXPECT_EQ(payload.biome.w, N);
    EXPECT_EQ(payload.biome.h, N);
}

TEST(Level2LuaTest, EdgeDescriptorsMatchElevationBoundary) {
    const MapTilePayload parent  = make_moderate_land_parent(1);
    const MapTilePayload payload = run_level2(parent, 2, TileCoord{2, 1, 0});

    ASSERT_EQ(static_cast<int>(payload.edges[0].elevation.size()), N);
    for (int i = 0; i < N; ++i)
        EXPECT_EQ(payload.edges[0].elevation[static_cast<std::size_t>(i)],
                  payload.elevation.at(i, 0)) << "N edge mismatch at i=" << i;
}

// M308's genuinely new functionality: 2-4 towns chained by trunk roads.
TEST(Level2LuaTest, EventuallyPlacesTownsAndRoadsAcrossSeveralEntropies) {
    const MapTilePayload parent = make_moderate_land_parent(1);
    bool found_town = false, found_road = false;
    for (std::uint64_t entropy = 1; entropy <= 20 && !(found_town && found_road); ++entropy) {
        const MapTilePayload payload = run_level2(parent, entropy, TileCoord{2, 1, 0});
        for (const auto& f : payload.features) {
            // MapBuilder::addCity()'s size_hint == "town" -> FeatureType::Town,
            // not ::City -- every site this script places is a plain town (no
            // capital concept at this level, see header note).
            if (f.type == FeatureType::Town) {
                found_town = true;
                EXPECT_FALSE(f.name.empty());
            }
            if (f.type == FeatureType::Road) {
                found_road = true;
                EXPECT_FALSE(f.name.empty());
                EXPECT_EQ(f.points.size(), 2u);
            }
        }
    }
    EXPECT_TRUE(found_town) << "no town placed across 20 entropies over all-land terrain";
    EXPECT_TRUE(found_road) << "no road placed across 20 entropies over all-land terrain";
}

// The chain shape: with N towns there must be exactly N-1 roads (a path
// graph), never N roads (which would imply a hub/cycle) and never fewer.
TEST(Level2LuaTest, RoadCountIsExactlyOneLessThanTownCount) {
    const MapTilePayload parent = make_moderate_land_parent(1);
    bool checked_at_least_one = false;
    for (std::uint64_t entropy = 1; entropy <= 20; ++entropy) {
        const MapTilePayload payload = run_level2(parent, entropy, TileCoord{2, 1, 0});
        const int towns = count_feature(payload, FeatureType::Town);
        const int roads = count_feature(payload, FeatureType::Road);
        if (towns == 0) continue;  // find_site() can miss even over all-land terrain; skip this entropy
        EXPECT_EQ(roads, towns - 1) << "entropy=" << entropy << " towns=" << towns << " roads=" << roads;
        checked_at_least_one = true;
    }
    EXPECT_TRUE(checked_at_least_one);
}

TEST(Level2LuaTest, NeverPlacesATownOrRoadOverOcean) {
    const MapTilePayload parent = make_all_ocean_parent(1);
    for (std::uint64_t entropy = 1; entropy <= 10; ++entropy) {
        const MapTilePayload payload = run_level2(parent, entropy, TileCoord{2, 0, 0});
        EXPECT_EQ(count_feature(payload, FeatureType::Town), 0) << "entropy=" << entropy;
        EXPECT_EQ(count_feature(payload, FeatureType::Road), 0) << "entropy=" << entropy;
    }
}

TEST(Level2LuaTest, DeterministicForSameEntropy) {
    const MapTilePayload parent = make_moderate_land_parent(1);
    const MapTilePayload a = run_level2(parent, 999, TileCoord{2, 1, 0});
    const MapTilePayload b = run_level2(parent, 999, TileCoord{2, 1, 0});

    EXPECT_EQ(a.elevation.data, b.elevation.data);
    ASSERT_EQ(a.features.size(), b.features.size());
    for (std::size_t i = 0; i < a.features.size(); ++i) {
        EXPECT_EQ(a.features[i].name, b.features[i].name);
        EXPECT_EQ(a.features[i].points, b.features[i].points);
    }
}

TEST(Level2LuaTest, MissingParentReturnsEmptyPayloadNotCrash) {
    MapGenContext ctx;
    ctx.tile        = TileCoord{2, 0, 0};
    ctx.entropy     = 1;
    ctx.sea_level_m = 0.0;
    ctx.parent      = nullptr;

    LuaSandbox sandbox;
    std::string error;
    const MapTilePayload payload = sandbox.executeMap(read_level2_lua(), ctx, &error);

    EXPECT_TRUE(error.empty());
    EXPECT_TRUE(payload.elevation.empty());
}
