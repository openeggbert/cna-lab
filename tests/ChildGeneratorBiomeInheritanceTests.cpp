// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
//
// M354 (MAP24, found 2026-07-10) — ChildGenerator (the generic C++ fallback
// used at any tile level with no registered Lua generator, currently levels
// 13/14/16/17/18) must propagate an ancestor's already-classified "city"
// zone down through generic descent, not silently re-derive a purely
// natural biome from elevation/temperature/moisture every level.
// BiomeClassifier has no "city" output at all (only city.lua's
// markUrbanCells(), M153, ever writes it, as a raster override) — found
// while generating a demonstrative city/street-level PNG for the user and
// discovering a "City" tile's streets rendered over forest/tundra one level
// down instead of over the city zone that produced them.

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>

#include "Map/BiomeClassifier.hpp"
#include "Map/ChildGenerator.hpp"
#include "Map/MapTilePayload.hpp"
#include "ZoneType.hpp"

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

// Uniform 1000 m / meadow everywhere — a realistic, non-urban parent (see
// NeighborhoodLuaTests.cpp's fixture of the same name/purpose: both must
// stay in sync with BiomeClassifier's real thresholds, not a hand-picked
// ZoneType ordinal, precisely because ZoneType::city IS ordinal 0 and a raw
// 0 used to be indistinguishable from "uninitialized").
MapTilePayload make_moderate_land_parent() {
    MapTilePayload p;
    p.culture = "nordic";
    p.elevation.w = p.elevation.h = N;
    p.elevation.data.assign(static_cast<std::size_t>(N * N), 1000.0f);
    p.biome.w = p.biome.h = N;
    p.biome.data.assign(static_cast<std::size_t>(N * N),
                         static_cast<std::uint8_t>(BiomeClassifier::classify(1000.0, 15.0, 0.4, 0.0)));
    return p;
}

// Same elevation field, but the western half (parent columns 0..31 of 64)
// of the biome grid is ZoneType::city — e.g. as if city.lua (level 12) had
// written it 1+ levels up.
MapTilePayload make_half_urban_parent() {
    MapTilePayload p = make_moderate_land_parent();
    for (int py = 0; py < N; ++py) {
        for (int px = 0; px < N / 2; ++px) {
            p.biome.data[static_cast<std::size_t>(py * N + px)] =
                static_cast<std::uint8_t>(ZoneType::city);
        }
    }
    return p;
}

} // namespace

TEST(ChildGeneratorBiomeInheritanceTest, NeverInventsCityFromAGenuinelyNonUrbanParent) {
    const MapTilePayload parent = make_moderate_land_parent();
    const ChildGenerator  gen(make_params());

    const MapTilePayload child = gen.generate(TileCoord{13, 0, 0}, &parent, 42ULL);

    for (const auto& b : child.biome.data)
        EXPECT_NE(static_cast<ZoneType>(b), ZoneType::city);
}

// cx = tile.x % 2 = 0 -> this child samples parent columns [0,32], entirely
// within make_half_urban_parent()'s city half at gx=0 (parent column 0
// exactly, no rounding ambiguity — mirrors NeighborhoodLuaTests.cpp's own
// interior-cell approach for the same reason).
TEST(ChildGeneratorBiomeInheritanceTest, InheritsCityFromParentBiomeInteriorOfUrbanHalf) {
    const MapTilePayload parent = make_half_urban_parent();
    const ChildGenerator  gen(make_params());

    const MapTilePayload child = gen.generate(TileCoord{13, 0, 0}, &parent, 42ULL);

    constexpr int gx = 0, gy = 0;
    EXPECT_EQ(static_cast<ZoneType>(child.biome.data[static_cast<std::size_t>(gy * N + gx)]),
              ZoneType::city);
}

// cx = tile.x % 2 = 1 -> this child samples parent columns [32,64], entirely
// outside the urban half at gx=W-1 (parent column 63, no rounding ambiguity).
TEST(ChildGeneratorBiomeInheritanceTest, DoesNotInheritCityOutsideTheUrbanParentHalf) {
    const MapTilePayload parent = make_half_urban_parent();
    const ChildGenerator  gen(make_params());

    const MapTilePayload child = gen.generate(TileCoord{13, 1, 0}, &parent, 42ULL);

    constexpr int gx = N - 1, gy = 0;
    EXPECT_NE(static_cast<ZoneType>(child.biome.data[static_cast<std::size_t>(gy * N + gx)]),
              ZoneType::city);
}

// The strongest ordering proof: an all-ocean parent ELEVATION (so this
// tile's own freshly-classified biome, before inheritance, would be ocean
// everywhere) combined with an all-city parent BIOME (a deliberately
// contrived, artificial combination — real generators never disagree this
// starkly between elevation and biome — used purely to isolate precedence).
// If inheritance ran before BiomeRefinement's coastal-beach pass, or before
// classify() itself, this would leave ocean/beach cells behind; it must not.
TEST(ChildGeneratorBiomeInheritanceTest, InheritedCityOverridesAFreshlyComputedOceanClassification) {
    MapTilePayload parent = make_moderate_land_parent();
    parent.elevation.data.assign(static_cast<std::size_t>(N * N), -500.0f);
    parent.biome.data.assign(static_cast<std::size_t>(N * N), static_cast<std::uint8_t>(ZoneType::city));
    const ChildGenerator gen(make_params());

    const MapTilePayload child = gen.generate(TileCoord{13, 0, 0}, &parent, 42ULL);

    for (const auto& b : child.biome.data)
        EXPECT_EQ(static_cast<ZoneType>(b), ZoneType::city);
}

TEST(ChildGeneratorBiomeInheritanceTest, EmptyParentBiomeGridDoesNotCrashOrInheritAnything) {
    // A hand-built parent that never populated biome at all (w=h=0, empty
    // data) — must degrade to "no inheritance", not crash on an out-of-
    // bounds clamp against a zero-sized grid.
    MapTilePayload parent;
    parent.culture = "nordic";
    parent.elevation.w = parent.elevation.h = N;
    parent.elevation.data.assign(static_cast<std::size_t>(N * N), 1000.0f);
    ASSERT_TRUE(parent.biome.empty());
    const ChildGenerator gen(make_params());

    const MapTilePayload child = gen.generate(TileCoord{13, 0, 0}, &parent, 42ULL);

    for (const auto& b : child.biome.data)
        EXPECT_NE(static_cast<ZoneType>(b), ZoneType::city);
}

TEST(ChildGeneratorBiomeInheritanceTest, DeterministicForSameEntropy) {
    const MapTilePayload parent = make_half_urban_parent();
    const ChildGenerator  gen(make_params());

    const MapTilePayload a = gen.generate(TileCoord{13, 0, 0}, &parent, 42ULL);
    const MapTilePayload b = gen.generate(TileCoord{13, 0, 0}, &parent, 42ULL);

    EXPECT_EQ(a.biome.data, b.biome.data);
}
