// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
//
// M089 — MapBuilder implementation tests (MAP6, Lua map-generator binding).

#include <gtest/gtest.h>

#include <vector>

#include "Map/BiomeClassifier.hpp"
#include "Map/ZoneCandidate.hpp"
#include "MapBuilder.hpp"
#include "ZoneType.hpp"

using namespace MeshWorld;
using namespace MeshWorld::Map;

TEST(MapBuilderTest, ConstructorSeedsTileAndEntropy) {
    MapBuilder builder(TileCoord{3, 5, 7}, 0x1234ULL, 0.0);
    EXPECT_EQ(builder.payload().tile, (TileCoord{3, 5, 7}));
    EXPECT_EQ(builder.payload().entropy, 0x1234ULL);
}

TEST(MapBuilderTest, SetBiomeFieldFillsAllThreeGridsAndDerivesBiome) {
    MapBuilder builder(TileCoord{0, 0, 0}, 1, /*sea_level_m=*/0.0);

    // 2x2 grid: two ocean cells (below sea level), two land cells.
    const std::vector<float> elevation   = {-10.0f, -10.0f, 100.0f, 100.0f};
    const std::vector<float> temperature = {20.0f, 20.0f, 20.0f, 20.0f};
    const std::vector<float> moisture    = {0.5f, 0.5f, 0.5f, 0.5f};
    builder.setBiomeField(2, 2, elevation, temperature, moisture);

    const auto& p = builder.payload();
    ASSERT_EQ(p.elevation.w, 2);
    ASSERT_EQ(p.elevation.h, 2);
    EXPECT_EQ(p.elevation.data, elevation);
    EXPECT_EQ(p.temperature.data, temperature);
    EXPECT_EQ(p.moisture.data, moisture);

    ASSERT_FALSE(p.biome.empty());
    for (int i = 0; i < 2; ++i) {
        const auto expected = BiomeClassifier::classify(elevation[static_cast<std::size_t>(i)],
                                                          temperature[static_cast<std::size_t>(i)],
                                                          moisture[static_cast<std::size_t>(i)], 0.0);
        EXPECT_EQ(static_cast<ZoneType>(p.biome.data[static_cast<std::size_t>(i)]), expected);
    }
    // Cells 0,1 are below sea level (depth=10m, shallow, moderate temp) ->
    // kelp_forest specifically since M236-M275's ocean-family subtyping
    // (MAP16, 2026-07-10), not generic ocean -- see BiomeClassifier.cpp.
    EXPECT_EQ(static_cast<ZoneType>(p.biome.data[0]), ZoneType::kelp_forest);
    EXPECT_EQ(static_cast<ZoneType>(p.biome.data[1]), ZoneType::kelp_forest);
}

// M095 — addContinent (added for the Lua planet.lua port; the C++
// PlanetGenerator pushes FeatureType::Continent features directly, without
// going through a builder helper).
TEST(MapBuilderTest, AddContinentAppendsContinentFeature) {
    MapBuilder builder(TileCoord{0, 0, 0}, 1, 0.0);
    builder.addContinent("Vorlandia", 100.0, 200.0);

    ASSERT_EQ(builder.payload().features.size(), 1u);
    const auto& f = builder.payload().features[0];
    EXPECT_EQ(f.type, FeatureType::Continent);
    EXPECT_EQ(f.name, "Vorlandia");
    ASSERT_EQ(f.points.size(), 1u);
    EXPECT_EQ(f.points[0][0], 100.0);
    EXPECT_EQ(f.points[0][1], 200.0);
}

TEST(MapBuilderTest, AddRiverAppendsRiverFeature) {
    MapBuilder builder(TileCoord{0, 0, 0}, 1, 0.0);
    builder.addRiver("Skarnfoss", {{0.0, 0.0}, {1.0, 2.0}, {3.0, 4.0}});

    ASSERT_EQ(builder.payload().features.size(), 1u);
    const auto& f = builder.payload().features[0];
    EXPECT_EQ(f.type, FeatureType::River);
    EXPECT_EQ(f.name, "Skarnfoss");
    ASSERT_EQ(f.points.size(), 3u);
    EXPECT_EQ(f.points[2][0], 3.0);
    EXPECT_EQ(f.points[2][1], 4.0);
}

TEST(MapBuilderTest, AddMountainRangeAppendsMountainRangeFeature) {
    MapBuilder builder(TileCoord{0, 0, 0}, 1, 0.0);
    builder.addMountainRange("Ironspine", {{5.0, 5.0}, {6.0, 7.0}});

    ASSERT_EQ(builder.payload().features.size(), 1u);
    EXPECT_EQ(builder.payload().features[0].type, FeatureType::MountainRange);
    EXPECT_EQ(builder.payload().features[0].name, "Ironspine");
}

TEST(MapBuilderTest, AddCityDefaultsToCityFeatureType) {
    MapBuilder builder(TileCoord{0, 0, 0}, 1, 0.0);
    builder.addCity("Vorhavn", 12.5, 34.5);

    ASSERT_EQ(builder.payload().features.size(), 1u);
    const auto& f = builder.payload().features[0];
    EXPECT_EQ(f.type, FeatureType::City);
    EXPECT_EQ(f.name, "Vorhavn");
    ASSERT_EQ(f.points.size(), 1u);
    EXPECT_EQ(f.points[0][0], 12.5);
    EXPECT_EQ(f.points[0][1], 34.5);
}

TEST(MapBuilderTest, AddCityWithTownHintUsesTownFeatureType) {
    MapBuilder builder(TileCoord{0, 0, 0}, 1, 0.0);
    builder.addCity("Little Elm", 1.0, 2.0, "town");

    ASSERT_EQ(builder.payload().features.size(), 1u);
    EXPECT_EQ(builder.payload().features[0].type, FeatureType::Town);
}

TEST(MapBuilderTest, AddBorderAppendsBorderFeatureNamedAfterCountry) {
    MapBuilder builder(TileCoord{0, 0, 0}, 1, 0.0);
    builder.addBorder("Aeland", {{0.0, 0.0}, {10.0, 0.0}, {10.0, 10.0}, {0.0, 10.0}});

    ASSERT_EQ(builder.payload().features.size(), 1u);
    const auto& f = builder.payload().features[0];
    EXPECT_EQ(f.type, FeatureType::Border);
    EXPECT_EQ(f.name, "Aeland");
    EXPECT_EQ(f.points.size(), 4u);
}

TEST(MapBuilderTest, AddRoadAppendsRoadFeature) {
    MapBuilder builder(TileCoord{0, 0, 0}, 1, 0.0);
    builder.addRoad("Kings Way", {{0.0, 0.0}, {5.0, 5.0}});

    ASSERT_EQ(builder.payload().features.size(), 1u);
    const auto& f = builder.payload().features[0];
    EXPECT_EQ(f.type, FeatureType::Road);
    EXPECT_EQ(f.name, "Kings Way");
    ASSERT_EQ(f.points.size(), 2u);
    EXPECT_EQ(f.points[1][0], 5.0);
    EXPECT_EQ(f.points[1][1], 5.0);
}

TEST(MapBuilderTest, AddLakeAppendsLakeFeature) {
    MapBuilder builder(TileCoord{0, 0, 0}, 1, 0.0);
    builder.addLake("Stillwater", {{1.0, 1.0}, {2.0, 1.0}, {2.0, 2.0}});

    ASSERT_EQ(builder.payload().features.size(), 1u);
    const auto& f = builder.payload().features[0];
    EXPECT_EQ(f.type, FeatureType::Lake);
    EXPECT_EQ(f.name, "Stillwater");
    EXPECT_EQ(f.points.size(), 3u);
}

TEST(MapBuilderTest, AddStreetAppendsStreetFeature) {
    MapBuilder builder(TileCoord{0, 0, 0}, 1, 0.0);
    builder.addStreet("Elm Street", {{0.0, 0.0}, {10.0, 0.0}});

    ASSERT_EQ(builder.payload().features.size(), 1u);
    const auto& f = builder.payload().features[0];
    EXPECT_EQ(f.type, FeatureType::Street);
    EXPECT_EQ(f.name, "Elm Street");
    ASSERT_EQ(f.points.size(), 2u);
    EXPECT_EQ(f.points[1][0], 10.0);
}

TEST(MapBuilderTest, AddParkAppendsParkFeatureAsASinglePoint) {
    MapBuilder builder(TileCoord{0, 0, 0}, 1, 0.0);
    builder.addPark("Central Park", 50.0, 60.0);

    ASSERT_EQ(builder.payload().features.size(), 1u);
    const auto& f = builder.payload().features[0];
    EXPECT_EQ(f.type, FeatureType::Park);
    EXPECT_EQ(f.name, "Central Park");
    ASSERT_EQ(f.points.size(), 1u);
    EXPECT_EQ(f.points[0][0], 50.0);
    EXPECT_EQ(f.points[0][1], 60.0);
}

TEST(MapBuilderTest, MarkUrbanCellsOverridesOnlyMaskedCellsToCity) {
    MapBuilder builder(TileCoord{0, 0, 0}, 1, /*sea_level_m=*/0.0);

    // 2x2, all land, all comfortably below any mountain threshold --
    // BiomeClassifier would classify every cell as ordinary land (not
    // ocean), never "city".
    const std::vector<float> elevation   = {50.0f, 50.0f, 50.0f, 50.0f};
    const std::vector<float> temperature = {20.0f, 20.0f, 20.0f, 20.0f};
    const std::vector<float> moisture    = {0.5f, 0.5f, 0.5f, 0.5f};
    builder.setBiomeField(2, 2, elevation, temperature, moisture);
    ASSERT_NE(static_cast<ZoneType>(builder.payload().biome.data[0]), ZoneType::city);

    builder.markUrbanCells({1, 0, 0, 1});

    const auto& biome = builder.payload().biome;
    EXPECT_EQ(static_cast<ZoneType>(biome.data[0]), ZoneType::city);
    EXPECT_NE(static_cast<ZoneType>(biome.data[1]), ZoneType::city);
    EXPECT_NE(static_cast<ZoneType>(biome.data[2]), ZoneType::city);
    EXPECT_EQ(static_cast<ZoneType>(biome.data[3]), ZoneType::city);
}

TEST(MapBuilderTest, MarkUrbanCellsIgnoresIndicesBeyondTheCurrentBiomeGrid) {
    MapBuilder builder(TileCoord{0, 0, 0}, 1, 0.0);
    builder.setBiomeField(2, 1, {50.0f, 50.0f}, {20.0f, 20.0f}, {0.5f, 0.5f});

    // Mask longer than the 2-cell grid -- must not crash or write out of bounds.
    builder.markUrbanCells({1, 1, 1, 1, 1});

    EXPECT_EQ(builder.payload().biome.data.size(), 2u);
    EXPECT_EQ(static_cast<ZoneType>(builder.payload().biome.data[0]), ZoneType::city);
    EXPECT_EQ(static_cast<ZoneType>(builder.payload().biome.data[1]), ZoneType::city);
}

TEST(MapBuilderTest, SetZoneCandidatesFillsWholeGridFromBiomeSize) {
    MapBuilder builder(TileCoord{0, 0, 0}, 1, 0.0);
    builder.setBiomeField(2, 2, {50.0f, 50.0f, 50.0f, 50.0f}, {20.0f, 20.0f, 20.0f, 20.0f},
                          {0.5f, 0.5f, 0.5f, 0.5f});

    builder.setZoneCandidates({static_cast<std::uint8_t>(ZoneCandidate::shop_street),
                               static_cast<std::uint8_t>(ZoneCandidate::park)});

    const auto& zc = builder.payload().zone_candidates;
    ASSERT_EQ(zc.w, 2);
    ASSERT_EQ(zc.h, 2);
    ASSERT_EQ(zc.data.size(), 4u);
    EXPECT_EQ(static_cast<ZoneCandidate>(zc.data[0]), ZoneCandidate::shop_street);
    EXPECT_EQ(static_cast<ZoneCandidate>(zc.data[1]), ZoneCandidate::park);
    // Cells beyond the provided mask default to `none`, not left uninitialized.
    EXPECT_EQ(static_cast<ZoneCandidate>(zc.data[2]), ZoneCandidate::none);
    EXPECT_EQ(static_cast<ZoneCandidate>(zc.data[3]), ZoneCandidate::none);
}

TEST(MapBuilderTest, SetZoneCandidatesIsANoOpBeforeSetBiomeField) {
    MapBuilder builder(TileCoord{0, 0, 0}, 1, 0.0);
    builder.setZoneCandidates({static_cast<std::uint8_t>(ZoneCandidate::square)});
    EXPECT_TRUE(builder.payload().zone_candidates.empty());
}

TEST(MapBuilderTest, SetEdgeWritesToCorrectEdgeIndex) {
    MapBuilder builder(TileCoord{0, 0, 0}, 1, 0.0);
    const std::vector<float> n_samples = {1.0f, 2.0f, 3.0f};
    const std::vector<float> e_samples = {4.0f, 5.0f};
    const std::vector<float> s_samples = {6.0f};
    const std::vector<float> w_samples = {7.0f, 8.0f, 9.0f, 10.0f};

    builder.setEdge("N", n_samples);
    builder.setEdge("E", e_samples);
    builder.setEdge("S", s_samples);
    builder.setEdge("W", w_samples);

    const auto& edges = builder.payload().edges;
    EXPECT_EQ(edges[0].elevation, n_samples);
    EXPECT_EQ(edges[1].elevation, e_samples);
    EXPECT_EQ(edges[2].elevation, s_samples);
    EXPECT_EQ(edges[3].elevation, w_samples);
}

TEST(MapBuilderTest, SetMetadataSetsGeneratorAndCulture) {
    MapBuilder builder(TileCoord{0, 0, 0}, 1, 0.0);
    builder.setMetadata("lua.map.planet.default", "nordic");

    EXPECT_EQ(builder.payload().generator, "lua.map.planet.default");
    EXPECT_EQ(builder.payload().culture, "nordic");
}

// M108 — a child's own N/W boundary (its two sides coinciding with the
// parent's own boundary) is overwritten by setBiomeField() to match the
// parent's edges, regardless of what the script computed there; the
// interior and the two non-owned boundary sides (E/S, owned by a sibling
// instead) are left untouched.
TEST(MapBuilderTest, SetBiomeFieldConstrainsBoundaryToParentEdges) {
    MapTilePayload parent;
    parent.tile = TileCoord{0, 0, 0};
    parent.elevation = FieldGrid{5, 5, {
        0, 1, 2, 3, 4,
        5, 6, 7, 8, 9,
        10, 11, 12, 13, 14,
        15, 16, 17, 18, 19,
        20, 21, 22, 23, 24,
    }};
    parent.edges[0].elevation = {0, 1, 2, 3, 4};       // N: row 0
    parent.edges[1].elevation = {4, 9, 14, 19, 24};    // E: col 4
    parent.edges[2].elevation = {20, 21, 22, 23, 24};  // S: row 4
    parent.edges[3].elevation = {0, 5, 10, 15, 20};    // W: col 0

    // Child (1,0,0): cx=0, cy=0 -> owns the parent's N and W edges.
    MapBuilder builder(TileCoord{1, 0, 0}, 99, 0.0, &parent);

    const std::vector<float> elevation(9, 1000.0f);  // deliberately "wrong" everywhere
    const std::vector<float> temperature(9, 20.0f);
    const std::vector<float> moisture(9, 0.5f);
    builder.setBiomeField(3, 3, elevation, temperature, moisture);

    const auto& p = builder.payload();
    // N row (gy=0): parent's N-edge lower half {0,1,2} resampled to width 3 -> itself.
    EXPECT_FLOAT_EQ(p.elevation.at(0, 0), 0.0f);
    EXPECT_FLOAT_EQ(p.elevation.at(1, 0), 1.0f);
    EXPECT_FLOAT_EQ(p.elevation.at(2, 0), 2.0f);
    // W col (gx=0): parent's W-edge lower half {0,5,10} resampled to height 3 -> itself.
    EXPECT_FLOAT_EQ(p.elevation.at(0, 1), 5.0f);
    EXPECT_FLOAT_EQ(p.elevation.at(0, 2), 10.0f);

    // Interior and the two non-owned sides (E, S) keep the script's own values.
    EXPECT_FLOAT_EQ(p.elevation.at(1, 1), 1000.0f);
    EXPECT_FLOAT_EQ(p.elevation.at(2, 1), 1000.0f);  // E edge — owned by the cx=1 sibling
    EXPECT_FLOAT_EQ(p.elevation.at(1, 2), 1000.0f);  // S edge — owned by the cy=1 sibling
    EXPECT_FLOAT_EQ(p.elevation.at(2, 2), 1000.0f);

    // Biome is re-derived at constrained cells (not left stale from elevation=1000).
    const auto expected_corner_biome = BiomeClassifier::classify(0.0, 20.0, 0.5, 0.0);
    EXPECT_EQ(static_cast<ZoneType>(p.biome.at(0, 0)), expected_corner_biome);
}

// Without a parent (level 0, the common case today), setBiomeField behaves
// exactly as before M108 — the pre-existing test above already covers this,
// this test just makes the "no parent -> no constraint" behavior explicit.
TEST(MapBuilderTest, SetBiomeFieldWithNoParentAppliesNoConstraint) {
    MapBuilder builder(TileCoord{0, 0, 0}, 1, 0.0);  // parent defaults to nullptr
    const std::vector<float> elevation = {1.0f, 2.0f, 3.0f, 4.0f};
    const std::vector<float> temperature(4, 20.0f);
    const std::vector<float> moisture(4, 0.5f);
    builder.setBiomeField(2, 2, elevation, temperature, moisture);

    EXPECT_EQ(builder.payload().elevation.data, elevation);
}

TEST(MapBuilderTest, PayloadIsIdempotentAcrossMultipleCalls) {
    MapBuilder builder(TileCoord{1, 2, 3}, 42, 0.0);
    builder.addCity("A", 1.0, 1.0);

    const auto& first  = builder.payload();
    const auto& second = builder.payload();
    EXPECT_EQ(first.features.size(), second.features.size());
    EXPECT_EQ(&first, &second);
}

// --- deriveEdgeCrossings (MAP10, M162) --------------------------------------
//
// A crossing is a feature POINT lying ON a tile boundary, not a segment
// passing through and beyond one — every Street/Road feature is validated
// (MapValidator) to stay strictly within its own tile's bounds, so a real
// script's streets can only ever touch an edge, never cross past it (see
// MapBuilder::deriveEdgeCrossings()'s own doc comment for the full
// reasoning). These fixtures construct points exactly ON the relevant
// boundary line, matching that reality.

TEST(MapBuilderTest, DeriveEdgeCrossingsFindsStreetTouchingNorthBoundary) {
    const TileCoord tile{18, 5, 5};
    MapBuilder builder(tile, 1, 0.0);
    builder.setBiomeField(2, 2, {10.0f, 10.0f, 10.0f, 10.0f}, {15.0f, 15.0f, 15.0f, 15.0f},
                          {0.4f, 0.4f, 0.4f, 0.4f});

    const auto   bounds      = tile.world_bounds();
    const double mid_x       = (bounds.min_x + bounds.max_x) * 0.5;
    const double interior_z  = (bounds.min_z + bounds.max_z) * 0.5;
    // Touches the N edge (z = bounds.min_z) at x = mid_x, then heads inward.
    builder.addStreet("Test St", {{mid_x, bounds.min_z}, {mid_x, interior_z}});
    builder.deriveEdgeCrossings();

    const auto& n = builder.payload().edges[0].crossings;
    ASSERT_EQ(n.size(), 1u);
    EXPECT_EQ(n[0].type, EdgeCrossingType::Road);
    EXPECT_NEAR(n[0].position, 0.5f, 0.01f);
    EXPECT_TRUE(builder.payload().edges[1].crossings.empty());
    EXPECT_TRUE(builder.payload().edges[2].crossings.empty());
    EXPECT_TRUE(builder.payload().edges[3].crossings.empty());
}

TEST(MapBuilderTest, DeriveEdgeCrossingsFindsRoadTouchingEveryEdge) {
    const TileCoord tile{18, 3, 3};
    MapBuilder builder(tile, 1, 0.0);
    builder.setBiomeField(2, 2, {10.0f, 10.0f, 10.0f, 10.0f}, {15.0f, 15.0f, 15.0f, 15.0f},
                          {0.4f, 0.4f, 0.4f, 0.4f});

    const auto bounds = tile.world_bounds();
    const auto mid_x  = (bounds.min_x + bounds.max_x) * 0.5;
    const auto mid_z  = (bounds.min_z + bounds.max_z) * 0.5;
    // Touches the midpoint of each of the 4 edges in turn, one point per edge.
    builder.addRoad("Loop", {{mid_x, bounds.min_z},      // N
                             {bounds.max_x, mid_z},      // E
                             {mid_x, bounds.max_z},      // S
                             {bounds.min_x, mid_z}});     // W
    builder.deriveEdgeCrossings();

    for (int i = 0; i < 4; ++i) {
        const auto& c = builder.payload().edges[static_cast<std::size_t>(i)].crossings;
        ASSERT_EQ(c.size(), 1u) << "edge " << i;
        EXPECT_EQ(c[0].type, EdgeCrossingType::Road) << "edge " << i;
        EXPECT_NEAR(c[0].position, 0.5f, 0.01f) << "edge " << i;
    }
}

TEST(MapBuilderTest, DeriveEdgeCrossingsIgnoresNonStreetNonRoadFeatures) {
    const TileCoord tile{18, 5, 5};
    MapBuilder builder(tile, 1, 0.0);
    builder.setBiomeField(2, 2, {10.0f, 10.0f, 10.0f, 10.0f}, {15.0f, 15.0f, 15.0f, 15.0f},
                          {0.4f, 0.4f, 0.4f, 0.4f});

    const auto   bounds     = tile.world_bounds();
    const double mid_x      = (bounds.min_x + bounds.max_x) * 0.5;
    const double interior_z = (bounds.min_z + bounds.max_z) * 0.5;
    // A river touching the exact same point a street would -- must NOT
    // produce a Road crossing (rivers are a separate concern, M110/M111).
    builder.addRiver("Test River", {{mid_x, bounds.min_z}, {mid_x, interior_z}});
    builder.deriveEdgeCrossings();

    EXPECT_TRUE(builder.payload().edges[0].crossings.empty());
}

TEST(MapBuilderTest, DeriveEdgeCrossingsFindsNoCrossingForAnInteriorStreet) {
    const TileCoord tile{18, 5, 5};
    MapBuilder builder(tile, 1, 0.0);
    builder.setBiomeField(2, 2, {10.0f, 10.0f, 10.0f, 10.0f}, {15.0f, 15.0f, 15.0f, 15.0f},
                          {0.4f, 0.4f, 0.4f, 0.4f});

    const auto bounds   = tile.world_bounds();
    const auto center_x = (bounds.min_x + bounds.max_x) * 0.5;
    const auto center_z = (bounds.min_z + bounds.max_z) * 0.5;
    const auto quarter  = (bounds.max_x - bounds.min_x) * 0.1;
    // A short street entirely inside the tile, nowhere near any boundary.
    builder.addStreet("Interior St", {{center_x - quarter, center_z}, {center_x + quarter, center_z}});
    builder.deriveEdgeCrossings();

    for (const auto& e : builder.payload().edges) EXPECT_TRUE(e.crossings.empty());
}

TEST(MapBuilderTest, DeriveEdgeCrossingsIsIdempotent) {
    const TileCoord tile{18, 5, 5};
    MapBuilder builder(tile, 1, 0.0);
    builder.setBiomeField(2, 2, {10.0f, 10.0f, 10.0f, 10.0f}, {15.0f, 15.0f, 15.0f, 15.0f},
                          {0.4f, 0.4f, 0.4f, 0.4f});

    const auto   bounds     = tile.world_bounds();
    const double mid_x      = (bounds.min_x + bounds.max_x) * 0.5;
    const double interior_z = (bounds.min_z + bounds.max_z) * 0.5;
    builder.addStreet("Test St", {{mid_x, bounds.min_z}, {mid_x, interior_z}});

    builder.deriveEdgeCrossings();
    builder.deriveEdgeCrossings();
    builder.deriveEdgeCrossings();

    EXPECT_EQ(builder.payload().edges[0].crossings.size(), 1u);
}

// M331 (MAP21) -- integration-level reachability: ZoneType::cave must be
// producible through the REAL setBiomeField() pipeline (real world
// coordinates + real entropy feeding Map::noise::worley_f1()), not just via
// a synthetic BiomeClassifier::classify() unit call with a hand-picked
// cavity_noise. A uniform mountain-tier elevation field over a 64x64 grid,
// tried across several tile positions/entropies (a real cave pocket is
// sparse -- not every tile is guaranteed to intersect one) -- at least one
// combination must produce at least one cave cell.
TEST(MapBuilderTest, SetBiomeFieldCanProduceCaveFromRealWorldCoordinates) {
    constexpr int N = 64;
    const std::vector<float> elevation(static_cast<std::size_t>(N * N), 4000.0f);   // mountain-tier
    const std::vector<float> temperature(static_cast<std::size_t>(N * N), 5.0f);    // above ice_cap/glacier
    const std::vector<float> moisture(static_cast<std::size_t>(N * N), 0.5f);

    bool found_cave = false;
    for (std::uint64_t entropy = 1; entropy <= 20 && !found_cave; ++entropy) {
        for (std::int64_t tx = 0; tx < 4 && !found_cave; ++tx) {
            MapBuilder builder(TileCoord{15, tx, tx}, entropy, 0.0);
            builder.setBiomeField(N, N, elevation, temperature, moisture);
            const auto& biome = builder.payload().biome;
            for (const auto ordinal : biome.data) {
                if (static_cast<ZoneType>(ordinal) == ZoneType::cave) { found_cave = true; break; }
            }
        }
    }
    EXPECT_TRUE(found_cave)
        << "expected at least one ZoneType::cave cell across 20 entropies x 4 tile "
           "positions over uniform mountain-tier terrain";
}

// Below mountain-tier elevation, the real pipeline never produces cave
// regardless of entropy/position -- cavity_noise only matters inside the
// mountain-tier branch (BiomeClassifier.cpp).
TEST(MapBuilderTest, SetBiomeFieldNeverProducesCaveBelowMountainTier) {
    constexpr int N = 16;
    const std::vector<float> elevation(static_cast<std::size_t>(N * N), 500.0f);  // well below MOUNTAIN_ELEV_M
    const std::vector<float> temperature(static_cast<std::size_t>(N * N), 10.0f);
    const std::vector<float> moisture(static_cast<std::size_t>(N * N), 0.5f);

    for (std::uint64_t entropy = 1; entropy <= 10; ++entropy) {
        MapBuilder builder(TileCoord{15, 0, 0}, entropy, 0.0);
        builder.setBiomeField(N, N, elevation, temperature, moisture);
        for (const auto ordinal : builder.payload().biome.data)
            EXPECT_NE(static_cast<ZoneType>(ordinal), ZoneType::cave) << "entropy=" << entropy;
    }
}
