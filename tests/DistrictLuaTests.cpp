// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
//
// M304 (MAP18) — structural tests for generators/lua/map/district.lua
// (level 11), mirroring CountryLuaTests.cpp's style (child-tile path,
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

std::string read_district_lua() {
    std::ifstream ifs("generators/lua/map/district.lua");
    std::ostringstream ss;
    ss << ifs.rdbuf();
    return ss.str();
}

MapTilePayload make_moderate_land_parent(std::uint64_t entropy) {
    MapTilePayload p;
    p.tile    = TileCoord{10, 0, 0};
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
    p.tile    = TileCoord{10, 0, 0};
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

MapTilePayload run_district(const MapTilePayload& parent, std::uint64_t entropy,
                             const TileCoord& tile, std::string* error_out = nullptr) {
    LuaSandbox sandbox;
    return sandbox.executeMap(read_district_lua(), make_ctx(parent, entropy, tile), error_out);
}

int count_feature(const MapTilePayload& payload, FeatureType type) {
    int n = 0;
    for (const auto& f : payload.features)
        if (f.type == type) ++n;
    return n;
}

} // namespace

TEST(DistrictLuaTest, FileReadsAndRunsWithoutError) {
    const std::string source = read_district_lua();
    ASSERT_FALSE(source.empty()) << "generators/lua/map/district.lua not found or empty";

    const MapTilePayload parent = make_moderate_land_parent(1);
    std::string error;
    const MapTilePayload payload = run_district(parent, 2, TileCoord{11, 1, 0}, &error);

    EXPECT_TRUE(error.empty()) << error;
    EXPECT_FALSE(payload.elevation.empty());
    EXPECT_EQ(payload.generator, "lua.map.child.level11.default");
    EXPECT_EQ(payload.culture, "nordic");
}

TEST(DistrictLuaTest, FieldShapesMatch64x64) {
    const MapTilePayload parent  = make_moderate_land_parent(1);
    const MapTilePayload payload = run_district(parent, 2, TileCoord{11, 1, 0});

    EXPECT_EQ(payload.elevation.w, N);
    EXPECT_EQ(payload.biome.w, N);
    EXPECT_EQ(payload.biome.h, N);
}

TEST(DistrictLuaTest, EdgeDescriptorsMatchElevationBoundary) {
    const MapTilePayload parent  = make_moderate_land_parent(1);
    const MapTilePayload payload = run_district(parent, 2, TileCoord{11, 1, 0});

    ASSERT_EQ(static_cast<int>(payload.edges[0].elevation.size()), N);
    for (int i = 0; i < N; ++i)
        EXPECT_EQ(payload.edges[0].elevation[static_cast<std::size_t>(i)],
                  payload.elevation.at(i, 0)) << "N edge mismatch at i=" << i;
}

// M304's genuinely new functionality: over solid land, all 4 quadrants
// should find a site nearly every time, giving 4 districts + 4 towns.
TEST(DistrictLuaTest, AllFourQuadrantsGetDistrictsOverSolidLand) {
    const MapTilePayload parent  = make_moderate_land_parent(1);
    const MapTilePayload payload = run_district(parent, 2, TileCoord{11, 1, 0});

    EXPECT_EQ(count_feature(payload, FeatureType::Border), 4);
    EXPECT_EQ(count_feature(payload, FeatureType::Town), 4);
    for (const auto& f : payload.features) {
        if (f.type == FeatureType::Border) {
            EXPECT_FALSE(f.name.empty());
            ASSERT_EQ(f.points.size(), 4u);
        }
        if (f.type == FeatureType::Town) EXPECT_FALSE(f.name.empty());
    }
}

// Each district's border must stay within a quarter of the tile's own
// bounds -- proves the quadrant split, not a whole-tile border like
// country.lua's.
TEST(DistrictLuaTest, DistrictBordersAreQuarterSizedNotWholeTile) {
    const TileCoord tile{11, 1, 0};
    const MapTilePayload parent  = make_moderate_land_parent(1);
    const MapTilePayload payload = run_district(parent, 2, tile);
    const WorldBounds b = tile.world_bounds();
    const double full_area = (b.max_x - b.min_x) * (b.max_z - b.min_z);

    int checked = 0;
    for (const auto& f : payload.features) {
        if (f.type != FeatureType::Border) continue;
        ASSERT_EQ(f.points.size(), 4u);
        const double w = std::abs(f.points[1][0] - f.points[0][0]);
        const double h = std::abs(f.points[2][1] - f.points[1][1]);
        const double area = w * h;
        EXPECT_NEAR(area, full_area / 4.0, full_area * 0.02) << f.name;
        ++checked;
    }
    EXPECT_GT(checked, 0);
}

TEST(DistrictLuaTest, NeverPlacesADistrictOrTownOverOcean) {
    const MapTilePayload parent = make_all_ocean_parent(1);
    for (std::uint64_t entropy = 1; entropy <= 10; ++entropy) {
        const MapTilePayload payload = run_district(parent, entropy, TileCoord{11, 0, 0});
        EXPECT_EQ(count_feature(payload, FeatureType::Border), 0) << "entropy=" << entropy;
        EXPECT_EQ(count_feature(payload, FeatureType::Town), 0) << "entropy=" << entropy;
    }
}

TEST(DistrictLuaTest, DeterministicForSameEntropy) {
    const MapTilePayload parent = make_moderate_land_parent(1);
    const MapTilePayload a = run_district(parent, 999, TileCoord{11, 1, 0});
    const MapTilePayload b = run_district(parent, 999, TileCoord{11, 1, 0});

    EXPECT_EQ(a.elevation.data, b.elevation.data);
    ASSERT_EQ(a.features.size(), b.features.size());
    for (std::size_t i = 0; i < a.features.size(); ++i) {
        EXPECT_EQ(a.features[i].name, b.features[i].name);
        EXPECT_EQ(a.features[i].points, b.features[i].points);
    }
}

TEST(DistrictLuaTest, MissingParentReturnsEmptyPayloadNotCrash) {
    MapGenContext ctx;
    ctx.tile        = TileCoord{11, 0, 0};
    ctx.entropy     = 1;
    ctx.sea_level_m = 0.0;
    ctx.parent      = nullptr;

    LuaSandbox sandbox;
    std::string error;
    const MapTilePayload payload = sandbox.executeMap(read_district_lua(), ctx, &error);

    EXPECT_TRUE(error.empty());
    EXPECT_TRUE(payload.elevation.empty());
}
