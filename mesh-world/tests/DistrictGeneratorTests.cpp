// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
//
// R105 — structural tests for DistrictGenerator, the native C++ port of
// generators/lua/map/district.lua (level 11). Mirrors DistrictLuaTests.cpp's
// own fixture/assertion style, adapted to call DistrictGenerator::generate()
// directly instead of running the Lua sandbox.

#include <gtest/gtest.h>

#include <cmath>
#include <cstdint>

#include "Map/MapTilePayload.hpp"
#include "generators/map/DistrictGenerator.hpp"

using namespace MeshWorld;
using namespace MeshWorld::Map;

namespace {

constexpr int N = 64;

PlanetParams make_params() {
    PlanetParams p;
    p.planet_size_m  = 22585000.0;
    p.continents_min = 5;
    p.continents_max = 12;
    p.sea_level_m    = 0.0;
    p.equator_temp_c = 30.0;
    p.pole_temp_c    = -20.0;
    return p;
}

MapTilePayload make_moderate_land_parent() {
    MapTilePayload p;
    p.elevation.w = p.elevation.h = N;
    p.elevation.data.assign(static_cast<std::size_t>(N * N), 1000.0f);
    p.culture = "nordic";
    return p;
}

MapTilePayload make_all_ocean_parent() {
    MapTilePayload p;
    p.elevation.w = p.elevation.h = N;
    p.elevation.data.assign(static_cast<std::size_t>(N * N), -2000.0f);
    p.culture = "nordic";
    return p;
}

int count_feature(const MapTilePayload& payload, FeatureType type) {
    int n = 0;
    for (const auto& f : payload.features)
        if (f.type == type) ++n;
    return n;
}

} // namespace

TEST(DistrictGeneratorTest, FieldShapesMatch64x64) {
    const MapTilePayload parent = make_moderate_land_parent();
    const DistrictGenerator gen(make_params());
    const MapTilePayload payload = gen.generate(TileCoord{11, 1, 0}, &parent, 2);

    EXPECT_EQ(payload.elevation.w, N);
    EXPECT_EQ(payload.biome.w, N);
    EXPECT_EQ(payload.biome.h, N);
    EXPECT_EQ(payload.generator, "cpp.map.district");
    EXPECT_EQ(payload.culture, "nordic");
}

// Over solid land, all 4 quadrants should find a site nearly every time,
// giving 4 districts + 4 towns.
TEST(DistrictGeneratorTest, AllFourQuadrantsGetDistrictsOverSolidLand) {
    const MapTilePayload parent = make_moderate_land_parent();
    const DistrictGenerator gen(make_params());
    const MapTilePayload payload = gen.generate(TileCoord{11, 1, 0}, &parent, 2);

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
// bounds -- proves the quadrant split, not a whole-tile border.
TEST(DistrictGeneratorTest, DistrictBordersAreQuarterSizedNotWholeTile) {
    const TileCoord tile{11, 1, 0};
    const MapTilePayload parent = make_moderate_land_parent();
    const DistrictGenerator gen(make_params());
    const MapTilePayload payload = gen.generate(tile, &parent, 2);
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

TEST(DistrictGeneratorTest, NeverPlacesADistrictOrTownOverOcean) {
    const MapTilePayload parent = make_all_ocean_parent();
    const DistrictGenerator gen(make_params());
    for (std::uint64_t entropy = 1; entropy <= 10; ++entropy) {
        const MapTilePayload payload = gen.generate(TileCoord{11, 0, 0}, &parent, entropy);
        EXPECT_EQ(count_feature(payload, FeatureType::Border), 0) << "entropy=" << entropy;
        EXPECT_EQ(count_feature(payload, FeatureType::Town), 0) << "entropy=" << entropy;
    }
}

TEST(DistrictGeneratorTest, DeterministicForSameEntropy) {
    const MapTilePayload parent = make_moderate_land_parent();
    const DistrictGenerator gen(make_params());
    const MapTilePayload a = gen.generate(TileCoord{11, 1, 0}, &parent, 999);
    const MapTilePayload b = gen.generate(TileCoord{11, 1, 0}, &parent, 999);

    EXPECT_EQ(a.elevation.data, b.elevation.data);
    ASSERT_EQ(a.features.size(), b.features.size());
    for (std::size_t i = 0; i < a.features.size(); ++i) {
        EXPECT_EQ(a.features[i].name, b.features[i].name);
        EXPECT_EQ(a.features[i].points, b.features[i].points);
    }
}
