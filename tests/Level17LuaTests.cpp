// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
//
// M301 (MAP18) — structural tests for generators/lua/map/level17.lua
// (level 17), mirroring NeighborhoodLuaTests.cpp's street coverage plus
// Level13LuaTests.cpp's M354 city-inheritance coverage (this level
// combines both, see level17.lua's own header).

#include <gtest/gtest.h>

#include <cmath>
#include <cstdint>
#include <fstream>
#include <sstream>

#include "LuaSandbox.hpp"
#include "Map/BiomeClassifier.hpp"
#include "Map/MapTilePayload.hpp"
#include "ZoneType.hpp"

using namespace MeshWorld;
using namespace MeshWorld::Map;

namespace {

constexpr int N = 64;

std::string read_level17_lua() {
    std::ifstream ifs("generators/lua/map/level17.lua");
    std::ostringstream ss;
    ss << ifs.rdbuf();
    return ss.str();
}

MapTilePayload make_moderate_land_parent(std::uint64_t entropy) {
    MapTilePayload p;
    p.tile    = TileCoord{16, 0, 0};
    p.entropy = entropy;
    p.culture = "nordic";

    p.elevation.w = p.elevation.h = N;
    p.elevation.data.assign(static_cast<std::size_t>(N * N), 1000.0f);
    p.temperature.w = p.temperature.h = N;
    p.temperature.data.assign(static_cast<std::size_t>(N * N), 15.0f);
    p.moisture.w = p.moisture.h = N;
    p.moisture.data.assign(static_cast<std::size_t>(N * N), 0.4f);
    p.biome.w = p.biome.h = N;
    p.biome.data.assign(static_cast<std::size_t>(N * N),
                         static_cast<std::uint8_t>(BiomeClassifier::classify(1000.0, 15.0, 0.4, 0.0)));
    for (auto& e : p.edges) e.elevation.assign(static_cast<std::size_t>(N), 1000.0f);
    return p;
}

MapTilePayload make_all_ocean_parent(std::uint64_t entropy) {
    MapTilePayload p;
    p.tile    = TileCoord{16, 0, 0};
    p.entropy = entropy;
    p.culture = "nordic";

    p.elevation.w = p.elevation.h = N;
    p.elevation.data.assign(static_cast<std::size_t>(N * N), -2000.0f);
    p.temperature.w = p.temperature.h = N;
    p.temperature.data.assign(static_cast<std::size_t>(N * N), 15.0f);
    p.moisture.w = p.moisture.h = N;
    p.moisture.data.assign(static_cast<std::size_t>(N * N), 0.4f);
    p.biome.w = p.biome.h = N;
    p.biome.data.assign(static_cast<std::size_t>(N * N),
                         static_cast<std::uint8_t>(BiomeClassifier::classify(-2000.0, 15.0, 0.4, 0.0)));
    for (auto& e : p.edges) e.elevation.assign(static_cast<std::size_t>(N), -2000.0f);
    return p;
}

MapTilePayload make_half_urban_parent(std::uint64_t entropy) {
    MapTilePayload p = make_moderate_land_parent(entropy);
    for (int py = 0; py < N; ++py)
        for (int px = 0; px < N / 2; ++px)
            p.biome.data[static_cast<std::size_t>(py * N + px)] =
                static_cast<std::uint8_t>(ZoneType::city);
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

MapTilePayload run_level17(const MapTilePayload& parent, std::uint64_t entropy,
                            const TileCoord& tile, std::string* error_out = nullptr) {
    LuaSandbox sandbox;
    return sandbox.executeMap(read_level17_lua(), make_ctx(parent, entropy, tile), error_out);
}

bool has_feature(const MapTilePayload& payload, FeatureType type) {
    for (const auto& f : payload.features)
        if (f.type == type) return true;
    return false;
}

int count_feature(const MapTilePayload& payload, FeatureType type) {
    int n = 0;
    for (const auto& f : payload.features)
        if (f.type == type) ++n;
    return n;
}

} // namespace

TEST(Level17LuaTest, FileReadsAndRunsWithoutError) {
    const std::string source = read_level17_lua();
    ASSERT_FALSE(source.empty()) << "generators/lua/map/level17.lua not found or empty";

    const MapTilePayload parent = make_moderate_land_parent(1);
    std::string error;
    const MapTilePayload payload = run_level17(parent, 2, TileCoord{17, 1, 0}, &error);

    EXPECT_TRUE(error.empty()) << error;
    EXPECT_FALSE(payload.elevation.empty());
    EXPECT_EQ(payload.generator, "lua.map.child.level17.default");
    EXPECT_EQ(payload.culture, "nordic");
}

TEST(Level17LuaTest, FieldShapesMatch64x64) {
    const MapTilePayload parent  = make_moderate_land_parent(1);
    const MapTilePayload payload = run_level17(parent, 2, TileCoord{17, 1, 0});
    EXPECT_EQ(payload.elevation.w, N);
    EXPECT_EQ(payload.biome.w, N);
}

// M155/neighborhood.lua's own convention: SAMPLE_STEP_CELLS=2 over a
// 64-cell tile -> 32 sampled steps + 1 trailing endpoint = 33 waypoints.
constexpr std::size_t kLevel17StreetWaypoints = 33u;

TEST(Level17LuaTest, PlacesAFinerStreetGridOverBuildableLand) {
    const MapTilePayload parent  = make_moderate_land_parent(1);
    const MapTilePayload payload = run_level17(parent, 2, TileCoord{17, 1, 0});

    int street_count = 0;
    for (const auto& f : payload.features) {
        if (f.type == FeatureType::Street) {
            ++street_count;
            EXPECT_FALSE(f.name.empty());
            ASSERT_EQ(f.points.size(), kLevel17StreetWaypoints);
        }
    }
    // STREET_SPACING_CELLS=2 over 64 cells -> 32 lines per axis, 64 total.
    EXPECT_EQ(street_count, 64);
}

TEST(Level17LuaTest, StreetGridIsFinerThanNeighborhoods) {
    constexpr int neighborhood_spacing = 4;
    constexpr int level17_spacing      = 2;
    const int neighborhood_lines = 2 * ((N - 1) / neighborhood_spacing + 1);
    const int level17_lines      = 2 * ((N - 1) / level17_spacing + 1);
    EXPECT_GT(level17_lines, neighborhood_lines);

    const MapTilePayload parent  = make_moderate_land_parent(1);
    const MapTilePayload payload = run_level17(parent, 2, TileCoord{17, 1, 0});
    EXPECT_EQ(count_feature(payload, FeatureType::Street), level17_lines);
}

TEST(Level17LuaTest, NeverPlacesStreetsOverOcean) {
    const MapTilePayload parent = make_all_ocean_parent(1);
    for (std::uint64_t entropy = 1; entropy <= 10; ++entropy) {
        const MapTilePayload payload = run_level17(parent, entropy, TileCoord{17, 0, 0});
        EXPECT_FALSE(has_feature(payload, FeatureType::Street)) << "entropy=" << entropy;
    }
}

TEST(Level17LuaTest, NeverInventsCityOverANonUrbanParent) {
    const MapTilePayload parent  = make_moderate_land_parent(1);
    const MapTilePayload payload = run_level17(parent, 2, TileCoord{17, 1, 0});
    for (const auto& b : payload.biome.data)
        EXPECT_NE(static_cast<ZoneType>(b), ZoneType::city);
}

TEST(Level17LuaTest, InheritsCityFromParentBiomeInteriorOfUrbanHalf) {
    const MapTilePayload parent  = make_half_urban_parent(1);
    const MapTilePayload payload = run_level17(parent, 2, TileCoord{17, 0, 0});  // cx=0: urban half

    constexpr int gx = 0, gy = 0;
    EXPECT_EQ(static_cast<ZoneType>(payload.biome.data[static_cast<std::size_t>(gy * N + gx)]),
              ZoneType::city);
}

TEST(Level17LuaTest, DoesNotInheritCityOutsideTheUrbanParentHalf) {
    const MapTilePayload parent  = make_half_urban_parent(1);
    const MapTilePayload payload = run_level17(parent, 2, TileCoord{17, 1, 0});  // cx=1: non-urban half

    constexpr int gx = N - 1, gy = 0;
    EXPECT_NE(static_cast<ZoneType>(payload.biome.data[static_cast<std::size_t>(gy * N + gx)]),
              ZoneType::city);
}

TEST(Level17LuaTest, InheritingCityDoesNotSuppressThisLevelsOwnStreetGrid) {
    const MapTilePayload parent  = make_half_urban_parent(1);
    const MapTilePayload payload = run_level17(parent, 2, TileCoord{17, 0, 0});
    EXPECT_TRUE(has_feature(payload, FeatureType::Street));
}

TEST(Level17LuaTest, DeterministicForSameEntropy) {
    const MapTilePayload parent = make_moderate_land_parent(1);
    const MapTilePayload a = run_level17(parent, 999, TileCoord{17, 1, 0});
    const MapTilePayload b = run_level17(parent, 999, TileCoord{17, 1, 0});
    EXPECT_EQ(a.elevation.data, b.elevation.data);
    EXPECT_EQ(a.biome.data, b.biome.data);
    ASSERT_EQ(a.features.size(), b.features.size());
    for (std::size_t i = 0; i < a.features.size(); ++i) {
        EXPECT_EQ(a.features[i].name, b.features[i].name);
        EXPECT_EQ(a.features[i].points, b.features[i].points);
    }
}

TEST(Level17LuaTest, MissingParentReturnsEmptyPayloadNotCrash) {
    MapGenContext ctx;
    ctx.tile        = TileCoord{17, 0, 0};
    ctx.entropy     = 1;
    ctx.sea_level_m = 0.0;
    ctx.parent      = nullptr;

    LuaSandbox sandbox;
    std::string error;
    const MapTilePayload payload = sandbox.executeMap(read_level17_lua(), ctx, &error);

    EXPECT_TRUE(error.empty());
    EXPECT_TRUE(payload.elevation.empty());
}
